/*
 * \brief  NOVA-specific VM-connection implementation
 * \author Alexander Boettcher
 * \author Christian Helmuth
 * \author Benjamin Lamowski
 * \date   2018-08-27
 */

/*
 * Copyright (C) 2018-2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/env.h>
#include <base/allocator.h>
#include <base/registry.h>
#include <base/sleep.h>
#include <base/attached_dataspace.h>
#include <vm_session/connection.h>
#include <vm_session/handler.h>
#include <util/noncopyable.h>

#include <cpu/vcpu_state.h>

/* Nova includes */
#include <nova/capability_space.h>
#include <nova/native_thread.h>
#include <nova/syscalls.h>
#include <nova_native_pd/client.h>
#include <nova_native_vcpu/nova_native_vcpu.h>

using namespace Genode;

using Exit_config = Vm_connection::Exit_config;
using Call_with_state = Vm_connection::Call_with_state;


/******************************
 ** NOVA vCPU implementation **
 ******************************/

struct Nova_vcpu : Rpc_client<Vm_session::Native_vcpu>, Noncopyable
{
	private:

		enum { VM_EXIT_STARTUP = 0xfe, VM_EXIT_RECALL = 0xff };

		using Vcpu_space = Id_space<Nova_vcpu>;

		static Vcpu_space &_vcpu_space()
		{
			static Vcpu_space instance;
			return instance;
		}

		Vcpu_space::Element _id_elem;

		struct Vcpu_id_space_exhausted : Exception { };

		Signal_dispatcher_base &_obj;
		Allocator              &_alloc;
		void                   *_ep_handler    { nullptr };
		void                   *_dispatching   { nullptr };
		bool                    _resume        { false };
		bool                    _last_resume   { true };

		Vcpu_state              _vcpu_state __attribute__((aligned(0x10))) { };

		inline void _read_nova_state(Nova::Utcb &);

		inline void _write_nova_state(Nova::Utcb &);

		addr_t _sm_sel() const {
			return Nova::NUM_INITIAL_PT_RESERVED + _id_elem.id().value * 4; }

		addr_t _ec_sel() const { return _sm_sel() + 1; }

		/**
		 * NOVA badge with 16-bit exit reason and 16-bit artificial vCPU ID
		 */
		struct Badge
		{
			uint32_t _value;

			Badge(unsigned long value)
			: _value((uint32_t)value) { }

			Badge(uint16_t vcpu_id, uint16_t exit_reason)
			: _value((uint32_t)(vcpu_id << 16) | (exit_reason & 0xffffu)) { }

			uint16_t exit_reason() const { return (uint16_t)( _value        & 0xffff); }
			uint16_t vcpu_id()     const { return (uint16_t)((_value >> 16) & 0xffff); }
			uint32_t value()       const { return _value; }
		};

		void _handle_exit(Nova::Utcb &);

		__attribute__((regparm(1))) static void _exit_entry(addr_t badge);

		Nova::Mtd _portal_mtd(unsigned exit, Exit_config const &config)
		{
			/* TODO define and implement omissions */
			(void)exit;
			(void)config;

			Genode::addr_t mtd = 0;

			mtd |= Nova::Mtd::ACDB;
			mtd |= Nova::Mtd::EBSD;
			mtd |= Nova::Mtd::EFL;
			mtd |= Nova::Mtd::ESP;
			mtd |= Nova::Mtd::EIP;
			mtd |= Nova::Mtd::DR;
			mtd |= Nova::Mtd::R8_R15;
			mtd |= Nova::Mtd::CR;
			mtd |= Nova::Mtd::CSSS;
			mtd |= Nova::Mtd::ESDS;
			mtd |= Nova::Mtd::FSGS;
			mtd |= Nova::Mtd::TR;
			mtd |= Nova::Mtd::LDTR;
			mtd |= Nova::Mtd::GDTR;
			mtd |= Nova::Mtd::IDTR;
			mtd |= Nova::Mtd::SYS;
			mtd |= Nova::Mtd::CTRL;
			mtd |= Nova::Mtd::INJ;
			mtd |= Nova::Mtd::STA;
			mtd |= Nova::Mtd::TSC;
			mtd |= Nova::Mtd::TSC_AUX;
			mtd |= Nova::Mtd::EFER;
			mtd |= Nova::Mtd::PDPTE;
			mtd |= Nova::Mtd::SYSCALL_SWAPGS;
			mtd |= Nova::Mtd::TPR;
			mtd |= Nova::Mtd::QUAL;
			mtd |= Nova::Mtd::XSAVE;
			mtd |= Nova::Mtd::FPU;

			return Nova::Mtd(mtd);
		}

		static Capability<Native_vcpu> _create_vcpu(Vm_connection &, Vcpu_handler_base &);

		static Signal_context_capability _create_exit_handler(Pd_session        &pd,
		                                                      Vcpu_handler_base &handler,
		                                                      uint16_t           vcpu_id,
		                                                      uint16_t           exit_reason,
		                                                      Nova::Mtd          mtd);
		/*
		 * Noncopyable
		 */
		Nova_vcpu(Nova_vcpu const &) = delete;
		Nova_vcpu &operator = (Nova_vcpu const &) = delete;

	public:

		Nova_vcpu(Env &env, Vm_connection &vm, Allocator &alloc,
		          Vcpu_handler_base &handler, Exit_config const &exit_config);

		void startup()
		{
			call<Rpc_startup>();
		}

		void with_state(Call_with_state &cw);
};


void Nova_vcpu::_read_nova_state(Nova::Utcb &utcb)
{
	using Segment = Genode::Vcpu_state::Segment;
	using Range   = Genode::Vcpu_state::Range;

	_vcpu_state.discharge();
	_vcpu_state.exit_reason = static_cast<unsigned int>(utcb.exit_reason);

	if (utcb.mtd & Nova::Mtd::FPU) {
		_vcpu_state.fpu.charge([&] (Vcpu_state::Fpu::State &fpu) {
			auto const fpu_size = unsigned(min(_vcpu_state.fpu.size(),
			                                   sizeof(utcb.fpu)));
			memcpy(&fpu, utcb.fpu, fpu_size);
			return fpu_size;
		});
	}

	if (utcb.mtd & Nova::Mtd::ACDB) {
		_vcpu_state.ax.charge(utcb.ax);
		_vcpu_state.cx.charge(utcb.cx);
		_vcpu_state.dx.charge(utcb.dx);
		_vcpu_state.bx.charge(utcb.bx);
	}

	if (utcb.mtd & Nova::Mtd::EBSD) {
		_vcpu_state.di.charge(utcb.di);
		_vcpu_state.si.charge(utcb.si);
		_vcpu_state.bp.charge(utcb.bp);
	}

	if (utcb.mtd & Nova::Mtd::EFL) _vcpu_state.flags.charge(utcb.flags);
	if (utcb.mtd & Nova::Mtd::ESP) _vcpu_state.sp.charge(utcb.sp);
	if (utcb.mtd & Nova::Mtd::DR)  _vcpu_state.dr7.charge(utcb.dr7);

	if (utcb.mtd & Nova::Mtd::EIP) {
		_vcpu_state.ip.charge(utcb.ip);
		_vcpu_state.ip_len.charge(utcb.instr_len);
	}

	if (utcb.mtd & Nova::Mtd::R8_R15) {
		_vcpu_state. r8.charge(utcb.read_r8());
		_vcpu_state. r9.charge(utcb.read_r9());
		_vcpu_state.r10.charge(utcb.read_r10());
		_vcpu_state.r11.charge(utcb.read_r11());
		_vcpu_state.r12.charge(utcb.read_r12());
		_vcpu_state.r13.charge(utcb.read_r13());
		_vcpu_state.r14.charge(utcb.read_r14());
		_vcpu_state.r15.charge(utcb.read_r15());
	}

	if (utcb.mtd & Nova::Mtd::CR) {
		_vcpu_state.cr0.charge(utcb.cr0);
		_vcpu_state.cr2.charge(utcb.cr2);
		_vcpu_state.cr3.charge(utcb.cr3);
		_vcpu_state.cr4.charge(utcb.cr4);
	}
	if (utcb.mtd & Nova::Mtd::CSSS) {
		_vcpu_state.cs.charge(Segment { .sel   = utcb.cs.sel,
		                            .ar    = utcb.cs.ar,
		                            .limit = utcb.cs.limit,
		                            .base  = utcb.cs.base });
		_vcpu_state.ss.charge(Segment { .sel   = utcb.ss.sel,
		                            .ar    = utcb.ss.ar,
		                            .limit = utcb.ss.limit,
		                            .base  = utcb.ss.base });
	}

	if (utcb.mtd & Nova::Mtd::ESDS) {
		_vcpu_state.es.charge(Segment { .sel   = utcb.es.sel,
		                            .ar    = utcb.es.ar,
		                            .limit = utcb.es.limit,
		                            .base  = utcb.es.base });
		_vcpu_state.ds.charge(Segment { .sel   = utcb.ds.sel,
		                            .ar    = utcb.ds.ar,
		                            .limit = utcb.ds.limit,
		                            .base  = utcb.ds.base });
	}

	if (utcb.mtd & Nova::Mtd::FSGS) {
		_vcpu_state.fs.charge(Segment { .sel   = utcb.fs.sel,
		                            .ar    = utcb.fs.ar,
		                            .limit = utcb.fs.limit,
		                            .base  = utcb.fs.base });
		_vcpu_state.gs.charge(Segment { .sel   = utcb.gs.sel,
		                            .ar    = utcb.gs.ar,
		                            .limit = utcb.gs.limit,
		                            .base  = utcb.gs.base });
	}

	if (utcb.mtd & Nova::Mtd::TR) {
		_vcpu_state.tr.charge(Segment { .sel   = utcb.tr.sel,
		                            .ar    = utcb.tr.ar,
		                            .limit = utcb.tr.limit,
		                            .base  = utcb.tr.base });
	}

	if (utcb.mtd & Nova::Mtd::LDTR) {
		_vcpu_state.ldtr.charge(Segment { .sel   = utcb.ldtr.sel,
		                              .ar    = utcb.ldtr.ar,
		                              .limit = utcb.ldtr.limit,
		                              .base  = utcb.ldtr.base });
	}

	if (utcb.mtd & Nova::Mtd::GDTR) {
		_vcpu_state.gdtr.charge(Range { .limit = utcb.gdtr.limit,
		                            .base  = utcb.gdtr.base });
	}

	if (utcb.mtd & Nova::Mtd::IDTR) {
		_vcpu_state.idtr.charge(Range { .limit = utcb.idtr.limit,
		                            .base  = utcb.idtr.base });
	}

	if (utcb.mtd & Nova::Mtd::SYS) {
		_vcpu_state.sysenter_cs.charge(utcb.sysenter_cs);
		_vcpu_state.sysenter_sp.charge(utcb.sysenter_sp);
		_vcpu_state.sysenter_ip.charge(utcb.sysenter_ip);
	}

	if (utcb.mtd & Nova::Mtd::QUAL) {
		_vcpu_state.qual_primary.charge(utcb.qual[0]);
		_vcpu_state.qual_secondary.charge(utcb.qual[1]);
	}

	if (utcb.mtd & Nova::Mtd::CTRL) {
		_vcpu_state.ctrl_primary.charge(utcb.ctrl[0]);
		_vcpu_state.ctrl_secondary.charge(utcb.ctrl[1]);
	}

	if (utcb.mtd & Nova::Mtd::INJ) {
		_vcpu_state.inj_info.charge(utcb.inj_info);
		_vcpu_state.inj_error.charge(utcb.inj_error);
	}

	if (utcb.mtd & Nova::Mtd::STA) {
		_vcpu_state.intr_state.charge(utcb.intr_state);
		_vcpu_state.actv_state.charge(utcb.actv_state);
	}

	if (utcb.mtd & Nova::Mtd::TSC) {
		_vcpu_state.tsc.charge(utcb.tsc_val);
		_vcpu_state.tsc_offset.charge(utcb.tsc_off);
	}

	if (utcb.mtd & Nova::Mtd::TSC_AUX) {
		_vcpu_state.tsc_aux.charge(utcb.tsc_aux);
	}

	if (utcb.mtd & Nova::Mtd::EFER) {
		_vcpu_state.efer.charge(utcb.read_efer());
	}

	if (utcb.mtd & Nova::Mtd::PDPTE) {
		_vcpu_state.pdpte_0.charge(utcb.pdpte[0]);
		_vcpu_state.pdpte_1.charge(utcb.pdpte[1]);
		_vcpu_state.pdpte_2.charge(utcb.pdpte[2]);
		_vcpu_state.pdpte_3.charge(utcb.pdpte[3]);
	}

	if (utcb.mtd & Nova::Mtd::SYSCALL_SWAPGS) {
		_vcpu_state.star.charge(utcb.read_star());
		_vcpu_state.lstar.charge(utcb.read_lstar());
		_vcpu_state.cstar.charge(utcb.read_cstar());
		_vcpu_state.fmask.charge(utcb.read_fmask());
		_vcpu_state.kernel_gs_base.charge(utcb.read_kernel_gs_base());
	}

	if (utcb.mtd & Nova::Mtd::TPR) {
		_vcpu_state.tpr.charge(utcb.read_tpr());
		_vcpu_state.tpr_threshold.charge(utcb.read_tpr_threshold());
	}

	if (utcb.mtd & Nova::Mtd::XSAVE) {
		_vcpu_state.xcr0.charge(utcb.xcr0);
		_vcpu_state.xss.charge(utcb.xss);
	}
}


void Nova_vcpu::_write_nova_state(Nova::Utcb &utcb)
{
	utcb.items = 0;
	utcb.mtd = 0;

	if (_vcpu_state.ax.charged() || _vcpu_state.cx.charged() ||
	    _vcpu_state.dx.charged() || _vcpu_state.bx.charged()) {
		utcb.mtd |= Nova::Mtd::ACDB;
		utcb.ax   = _vcpu_state.ax.value();
		utcb.cx   = _vcpu_state.cx.value();
		utcb.dx   = _vcpu_state.dx.value();
		utcb.bx   = _vcpu_state.bx.value();
	}

	if (_vcpu_state.bp.charged() || _vcpu_state.di.charged() || _vcpu_state.si.charged()) {
		utcb.mtd |= Nova::Mtd::EBSD;
		utcb.di   = _vcpu_state.di.value();
		utcb.si   = _vcpu_state.si.value();
		utcb.bp   = _vcpu_state.bp.value();
	}

	if (_vcpu_state.flags.charged()) {
		utcb.mtd |= Nova::Mtd::EFL;
		utcb.flags = _vcpu_state.flags.value();
	}

	if (_vcpu_state.sp.charged()) {
		utcb.mtd |= Nova::Mtd::ESP;
		utcb.sp = _vcpu_state.sp.value();
	}

	if (_vcpu_state.ip.charged()) {
		utcb.mtd |= Nova::Mtd::EIP;
		utcb.ip = _vcpu_state.ip.value();
		utcb.instr_len = _vcpu_state.ip_len.value();
	}

	if (_vcpu_state.dr7.charged()) {
		utcb.mtd |= Nova::Mtd::DR;
		utcb.dr7 = _vcpu_state.dr7.value();
	}

	if (_vcpu_state.r8 .charged() || _vcpu_state.r9 .charged() ||
	    _vcpu_state.r10.charged() || _vcpu_state.r11.charged() ||
	    _vcpu_state.r12.charged() || _vcpu_state.r13.charged() ||
	    _vcpu_state.r14.charged() || _vcpu_state.r15.charged()) {

		utcb.mtd |= Nova::Mtd::R8_R15;
		utcb.write_r8 (_vcpu_state.r8.value());
		utcb.write_r9 (_vcpu_state.r9.value());
		utcb.write_r10(_vcpu_state.r10.value());
		utcb.write_r11(_vcpu_state.r11.value());
		utcb.write_r12(_vcpu_state.r12.value());
		utcb.write_r13(_vcpu_state.r13.value());
		utcb.write_r14(_vcpu_state.r14.value());
		utcb.write_r15(_vcpu_state.r15.value());
	}

	if (_vcpu_state.cr0.charged() || _vcpu_state.cr2.charged() ||
	    _vcpu_state.cr3.charged() || _vcpu_state.cr4.charged()) {
		utcb.mtd |= Nova::Mtd::CR;
		utcb.cr0 = _vcpu_state.cr0.value();
		utcb.cr2 = _vcpu_state.cr2.value();
		utcb.cr3 = _vcpu_state.cr3.value();
		utcb.cr4 = _vcpu_state.cr4.value();
	}

	if (_vcpu_state.cs.charged() || _vcpu_state.ss.charged()) {
		utcb.mtd |= Nova::Mtd::CSSS;
		utcb.cs.sel   = _vcpu_state.cs.value().sel;
		utcb.cs.ar    = _vcpu_state.cs.value().ar;
		utcb.cs.limit = _vcpu_state.cs.value().limit;
		utcb.cs.base  = _vcpu_state.cs.value().base;

		utcb.ss.sel   = _vcpu_state.ss.value().sel;
		utcb.ss.ar    = _vcpu_state.ss.value().ar;
		utcb.ss.limit = _vcpu_state.ss.value().limit;
		utcb.ss.base  = _vcpu_state.ss.value().base;
	}

	if (_vcpu_state.es.charged() || _vcpu_state.ds.charged()) {
		utcb.mtd     |= Nova::Mtd::ESDS;
		utcb.es.sel   = _vcpu_state.es.value().sel;
		utcb.es.ar    = _vcpu_state.es.value().ar;
		utcb.es.limit = _vcpu_state.es.value().limit;
		utcb.es.base  = _vcpu_state.es.value().base;

		utcb.ds.sel   = _vcpu_state.ds.value().sel;
		utcb.ds.ar    = _vcpu_state.ds.value().ar;
		utcb.ds.limit = _vcpu_state.ds.value().limit;
		utcb.ds.base  = _vcpu_state.ds.value().base;
	}

	if (_vcpu_state.fs.charged() || _vcpu_state.gs.charged()) {
		utcb.mtd     |= Nova::Mtd::FSGS;
		utcb.fs.sel   = _vcpu_state.fs.value().sel;
		utcb.fs.ar    = _vcpu_state.fs.value().ar;
		utcb.fs.limit = _vcpu_state.fs.value().limit;
		utcb.fs.base  = _vcpu_state.fs.value().base;

		utcb.gs.sel   = _vcpu_state.gs.value().sel;
		utcb.gs.ar    = _vcpu_state.gs.value().ar;
		utcb.gs.limit = _vcpu_state.gs.value().limit;
		utcb.gs.base  = _vcpu_state.gs.value().base;
	}

	if (_vcpu_state.tr.charged()) {
		utcb.mtd     |= Nova::Mtd::TR;
		utcb.tr.sel   = _vcpu_state.tr.value().sel;
		utcb.tr.ar    = _vcpu_state.tr.value().ar;
		utcb.tr.limit = _vcpu_state.tr.value().limit;
		utcb.tr.base  = _vcpu_state.tr.value().base;
	}

	if (_vcpu_state.ldtr.charged()) {
		utcb.mtd       |= Nova::Mtd::LDTR;
		utcb.ldtr.sel   = _vcpu_state.ldtr.value().sel;
		utcb.ldtr.ar    = _vcpu_state.ldtr.value().ar;
		utcb.ldtr.limit = _vcpu_state.ldtr.value().limit;
		utcb.ldtr.base  = _vcpu_state.ldtr.value().base;
	}

	if (_vcpu_state.gdtr.charged()) {
		utcb.mtd       |= Nova::Mtd::GDTR;
		utcb.gdtr.limit = _vcpu_state.gdtr.value().limit;
		utcb.gdtr.base  = _vcpu_state.gdtr.value().base;
	}

	if (_vcpu_state.idtr.charged()) {
		utcb.mtd       |= Nova::Mtd::IDTR;
		utcb.idtr.limit = _vcpu_state.idtr.value().limit;
		utcb.idtr.base  = _vcpu_state.idtr.value().base;
	}

	if (_vcpu_state.sysenter_cs.charged() || _vcpu_state.sysenter_sp.charged() ||
	    _vcpu_state.sysenter_ip.charged()) {
		utcb.mtd        |= Nova::Mtd::SYS;
		utcb.sysenter_cs = _vcpu_state.sysenter_cs.value();
		utcb.sysenter_sp = _vcpu_state.sysenter_sp.value();
		utcb.sysenter_ip = _vcpu_state.sysenter_ip.value();
	}

	if (_vcpu_state.ctrl_primary.charged() || _vcpu_state.ctrl_secondary.charged()) {
		utcb.mtd     |= Nova::Mtd::CTRL;
		utcb.ctrl[0]  = _vcpu_state.ctrl_primary.value();
		utcb.ctrl[1]  = _vcpu_state.ctrl_secondary.value();
	}

	if (_vcpu_state.inj_info.charged() || _vcpu_state.inj_error.charged()) {
		utcb.mtd      |= Nova::Mtd::INJ;
		utcb.inj_info  = _vcpu_state.inj_info.value();
		utcb.inj_error = _vcpu_state.inj_error.value();
	}

	if (_vcpu_state.intr_state.charged() || _vcpu_state.actv_state.charged()) {
		utcb.mtd        |= Nova::Mtd::STA;
		utcb.intr_state  = _vcpu_state.intr_state.value();
		utcb.actv_state  = _vcpu_state.actv_state.value();
	}

	if (_vcpu_state.tsc.charged() || _vcpu_state.tsc_offset.charged()) {
		utcb.mtd     |= Nova::Mtd::TSC;
		utcb.tsc_val  = _vcpu_state.tsc.value();
		utcb.tsc_off  = _vcpu_state.tsc_offset.value();
	}

	if (_vcpu_state.tsc_aux.charged()) {
		utcb.mtd     |= Nova::Mtd::TSC_AUX;
		utcb.tsc_aux  = _vcpu_state.tsc_aux.value();
	}

	if (_vcpu_state.efer.charged()) {
		utcb.mtd |= Nova::Mtd::EFER;
		utcb.write_efer(_vcpu_state.efer.value());
	}

	if (_vcpu_state.pdpte_0.charged() || _vcpu_state.pdpte_1.charged() ||
	    _vcpu_state.pdpte_2.charged() || _vcpu_state.pdpte_3.charged()) {

		utcb.mtd |= Nova::Mtd::PDPTE;
		utcb.pdpte[0] = (Nova::mword_t)_vcpu_state.pdpte_0.value();
		utcb.pdpte[1] = (Nova::mword_t)_vcpu_state.pdpte_1.value();
		utcb.pdpte[2] = (Nova::mword_t)_vcpu_state.pdpte_2.value();
		utcb.pdpte[3] = (Nova::mword_t)_vcpu_state.pdpte_3.value();
	}

	if (_vcpu_state.star.charged() || _vcpu_state.lstar.charged() ||
	    _vcpu_state.cstar.charged() || _vcpu_state.fmask.charged() ||
	    _vcpu_state.kernel_gs_base.charged()) {

		utcb.mtd  |= Nova::Mtd::SYSCALL_SWAPGS;
		utcb.write_star (_vcpu_state.star.value());
		utcb.write_lstar(_vcpu_state.lstar.value());
		utcb.write_cstar(_vcpu_state.cstar.value());
		utcb.write_fmask(_vcpu_state.fmask.value());
		utcb.write_kernel_gs_base(_vcpu_state.kernel_gs_base.value());
	}

	if (_vcpu_state.tpr.charged() || _vcpu_state.tpr_threshold.charged()) {
		utcb.mtd |= Nova::Mtd::TPR;
		utcb.write_tpr(_vcpu_state.tpr.value());
		utcb.write_tpr_threshold(_vcpu_state.tpr_threshold.value());
	}

	if (_vcpu_state.xcr0.charged() || _vcpu_state.xss.charged()) {
		utcb.xcr0 = _vcpu_state.xcr0.value();
		utcb.xss  = _vcpu_state.xss.value();

		utcb.mtd |= Nova::Mtd::XSAVE;
	}

	if (_vcpu_state.fpu.charged()) {
		utcb.mtd |= Nova::Mtd::FPU;
		_vcpu_state.fpu.with_state([&] (Vcpu_state::Fpu::State const &fpu) {
			memcpy(utcb.fpu, &fpu, sizeof(fpu));
		});
	}
}


/*
 * Do not touch the UTCB before _read_nova_state() and after
 * _write_nova_state(), particularly not by logging diagnostics.
 */
void Nova_vcpu::_handle_exit(Nova::Utcb &utcb)
{
	 if (utcb.exit_reason == VM_EXIT_RECALL) {
		/*
		 * A recall exit is only requested from an asynchronous Signal to the
		 * vCPU Handler. In that case, VM_EXIT_RECALL has already been processed
		 * asynchronously by getting and setting the state via system calls and
		 * the regular exit does not need to be processed.
		 */
		utcb.mtd   = 0;
		utcb.items = 0;
		return;
	}

	_read_nova_state(utcb);

	try {
		_dispatching = Thread::myself();
		/* call dispatch handler */
		_obj.dispatch(1);
		_dispatching = nullptr;
	} catch (...) {
		_dispatching = nullptr;
		throw;
	}

	/* reply to NOVA and transfer vCPU state */
	_write_nova_state(utcb);
}


void Nova_vcpu::with_state(Call_with_state &cw)
{
	Thread *myself   = Thread::myself();
	bool remote      = (_dispatching != myself);
	Nova::Utcb &utcb = *reinterpret_cast<Nova::Utcb *>(myself->utcb());

	if (remote) {
		if (Thread::myself() != _ep_handler) {
			error("vCPU state requested outside of vcpu_handler EP");
			sleep_forever();
		};

		Exit_config config { };
		Nova::Mtd mtd = _portal_mtd(0, config);

		uint8_t res = Nova::ec_ctrl(Nova::EC_GET_VCPU_STATE, _ec_sel(), mtd.value());

		if (res != Nova::NOVA_OK) {
			error("Getting vCPU state failed with: ", res);
			sleep_forever();
		};

		_read_nova_state(utcb);
	}

	_resume = cw.call_with_state(_vcpu_state);

	if (remote) {
		_write_nova_state(utcb);
		/*
		 * A recall is needed
		 * a) when the vCPU should be stopped or
		 * b) when the vCPU should be resumed from a stopped state.
		 */
		bool recall = !(_resume && _last_resume);

		uint8_t res = Nova::ec_ctrl(Nova::EC_SET_VCPU_STATE, _ec_sel(), recall);

		if (res != Nova::NOVA_OK) {
			error("Setting vCPU state failed with: ", res);
			sleep_forever();
		};

		/*
		 * Resume the vCPU and indicate to the next exit if state
		 * needs to be synced or not.
		 */
		if (_resume)
			Nova::sm_ctrl(_sm_sel(), Nova::SEMAPHORE_UP);
	}
}


static void nova_reply(Thread &myself, Nova::Utcb &utcb, auto &&... args)
{
	Receive_window &rcv_window = myself.native_thread().server_rcv_window;

	/* reset receive window to values expected by RPC server code */
	rcv_window.prepare_rcv_window(utcb);

	Nova::reply(myself.stack_top(), args...);
}


void Nova_vcpu::_exit_entry(addr_t badge)
{
	Thread     &myself = *Thread::myself();
	Nova::Utcb &utcb   = *reinterpret_cast<Nova::Utcb *>(myself.utcb());

	Vcpu_space::Id const vcpu_id { Badge(badge).vcpu_id() };

	_vcpu_space().apply<Nova_vcpu>(vcpu_id,
		[&] (Nova_vcpu &vcpu) {
			vcpu._handle_exit(utcb);

			vcpu._last_resume = vcpu._resume;
			if (vcpu._resume) {
				nova_reply(myself, utcb);
			} else {
				nova_reply(myself, utcb, vcpu._sm_sel());
			}
		},
		[&] /* missing */ {
			/* somebody called us directly ? ... ignore/deny */
			utcb.items = 0;
			utcb.mtd   = 0;
			nova_reply(myself, utcb);
		});
}


Signal_context_capability Nova_vcpu::_create_exit_handler(Pd_session        &pd,
                                                          Vcpu_handler_base &handler,
                                                          uint16_t           vcpu_id,
                                                          uint16_t           exit_reason,
                                                          Nova::Mtd          mtd)
{
	Thread *tep = reinterpret_cast<Thread *>(&handler.rpc_ep());

	Native_capability thread_cap = Capability_space::import(tep->native_thread().ec_sel);

	Nova_native_pd_client native_pd { pd.native_pd() };

	Native_capability vm_exit_cap =
		native_pd.alloc_rpc_cap(thread_cap, (addr_t)Nova_vcpu::_exit_entry, mtd.value());

	Badge const badge { vcpu_id, exit_reason };
	native_pd.imprint_rpc_cap(vm_exit_cap, badge.value());

	return reinterpret_cap_cast<Signal_context>(vm_exit_cap);
}


Capability<Vm_session::Native_vcpu> Nova_vcpu::_create_vcpu(Vm_connection     &vm,
                                                            Vcpu_handler_base &handler)
{
	Thread &tep { *reinterpret_cast<Thread *>(&handler.rpc_ep()) };

	return vm.create_vcpu(tep.cap());
}


Nova_vcpu::Nova_vcpu(Env &env, Vm_connection &vm, Allocator &alloc,
                     Vcpu_handler_base &handler, Exit_config const &exit_config)
:
	Rpc_client<Native_vcpu>(_create_vcpu(vm, handler)),
	_id_elem(*this, _vcpu_space()), _obj(handler), _alloc(alloc)
{
	/*
	 * XXX can be alleviated by managing ID values with Bit_allocator
	 * that allocates lowest free index in dynamic scenarios
	 */
	if (_id_elem.id().value > 0xffff)
		throw Vcpu_id_space_exhausted();

	_ep_handler = reinterpret_cast<Thread *>(&handler.rpc_ep());

	uint16_t const vcpu_id =  (uint16_t)_id_elem.id().value;

	Signal_context_capability dontcare_exit =
		_create_exit_handler(env.pd(), handler, vcpu_id, 0x100, Nova::Mtd(Nova::Mtd::EIP));

	for (unsigned i = 0; i < Nova::NUM_INITIAL_VCPU_PT; ++i) {
		Signal_context_capability signal_exit;

		Nova::Mtd mtd = _portal_mtd(i, exit_config);
		if (mtd.value()) {
			signal_exit = _create_exit_handler(env.pd(), handler, vcpu_id, (uint16_t)i, mtd);
		} else {
			signal_exit = dontcare_exit;
		}

		call<Rpc_exit_handler>(i, signal_exit);
	}
}


/**************
 ** vCPU API **
 **************/

void Vm_connection::Vcpu::_with_state(Call_with_state &cw) { static_cast<Nova_vcpu &>(_native_vcpu).with_state(cw); }


Vm_connection::Vcpu::Vcpu(Vm_connection &vm, Allocator &alloc,
                          Vcpu_handler_base &handler, Exit_config const &exit_config)
:
	_native_vcpu(*new (alloc) Nova_vcpu(vm._env, vm, alloc, handler, exit_config))
{
	static_cast<Nova_vcpu &>(_native_vcpu).startup();
}
