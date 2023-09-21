/*
 * \brief  NOVA-specific VM-connection implementation
 * \author Alexander Boettcher
 * \author Christian Helmuth
 * \date   2018-08-27
 */

/*
 * Copyright (C) 2018-2021 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/env.h>
#include <base/allocator.h>
#include <base/registry.h>
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


/******************************
 ** NOVA vCPU implementation **
 ******************************/

struct Nova_vcpu : Rpc_client<Vm_session::Native_vcpu>, Noncopyable
{
	private:

		typedef Id_space<Nova_vcpu> Vcpu_space;

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
		bool                    _block         { true };

		Vcpu_state              _vcpu_state __attribute__((aligned(0x10))) { };

		enum Remote_state_requested {
			NONE = 0,
			PAUSE = 1,
			RUN = 2
		} _remote { NONE };

		inline void _read_nova_state(Nova::Utcb &, unsigned exit_reason);

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

		bool _handle_exit(Nova::Utcb &, uint16_t exit_reason);

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

		void run();

		void pause();

		Vcpu_state & state() { return _vcpu_state; }
};


void Nova_vcpu::_read_nova_state(Nova::Utcb &utcb, unsigned exit_reason)
{
	typedef Genode::Vcpu_state::Segment Segment;
	typedef Genode::Vcpu_state::Range Range;

	state().discharge();
	state().exit_reason = exit_reason;

	if (utcb.mtd & Nova::Mtd::FPU) {
		state().fpu.charge([&] (Vcpu_state::Fpu::State &fpu) {
			memcpy(&fpu, utcb.fpu, sizeof(fpu));
		});
	}

	if (utcb.mtd & Nova::Mtd::ACDB) {
		state().ax.charge(utcb.ax);
		state().cx.charge(utcb.cx);
		state().dx.charge(utcb.dx);
		state().bx.charge(utcb.bx);
	}

	if (utcb.mtd & Nova::Mtd::EBSD) {
		state().di.charge(utcb.di);
		state().si.charge(utcb.si);
		state().bp.charge(utcb.bp);
	}

	if (utcb.mtd & Nova::Mtd::EFL) state().flags.charge(utcb.flags);
	if (utcb.mtd & Nova::Mtd::ESP) state().sp.charge(utcb.sp);
	if (utcb.mtd & Nova::Mtd::DR)  state().dr7.charge(utcb.dr7);

	if (utcb.mtd & Nova::Mtd::EIP) {
		state().ip.charge(utcb.ip);
		state().ip_len.charge(utcb.instr_len);
	}

	if (utcb.mtd & Nova::Mtd::R8_R15) {
		state(). r8.charge(utcb.read_r8());
		state(). r9.charge(utcb.read_r9());
		state().r10.charge(utcb.read_r10());
		state().r11.charge(utcb.read_r11());
		state().r12.charge(utcb.read_r12());
		state().r13.charge(utcb.read_r13());
		state().r14.charge(utcb.read_r14());
		state().r15.charge(utcb.read_r15());
	}

	if (utcb.mtd & Nova::Mtd::CR) {
		state().cr0.charge(utcb.cr0);
		state().cr2.charge(utcb.cr2);
		state().cr3.charge(utcb.cr3);
		state().cr4.charge(utcb.cr4);
	}
	if (utcb.mtd & Nova::Mtd::CSSS) {
		state().cs.charge(Segment { .sel   = utcb.cs.sel,
		                            .ar    = utcb.cs.ar,
		                            .limit = utcb.cs.limit,
		                            .base  = utcb.cs.base });
		state().ss.charge(Segment { .sel   = utcb.ss.sel,
		                            .ar    = utcb.ss.ar,
		                            .limit = utcb.ss.limit,
		                            .base  = utcb.ss.base });
	}

	if (utcb.mtd & Nova::Mtd::ESDS) {
		state().es.charge(Segment { .sel   = utcb.es.sel,
		                            .ar    = utcb.es.ar,
		                            .limit = utcb.es.limit,
		                            .base  = utcb.es.base });
		state().ds.charge(Segment { .sel   = utcb.ds.sel,
		                            .ar    = utcb.ds.ar,
		                            .limit = utcb.ds.limit,
		                            .base  = utcb.ds.base });
	}

	if (utcb.mtd & Nova::Mtd::FSGS) {
		state().fs.charge(Segment { .sel   = utcb.fs.sel,
		                            .ar    = utcb.fs.ar,
		                            .limit = utcb.fs.limit,
		                            .base  = utcb.fs.base });
		state().gs.charge(Segment { .sel   = utcb.gs.sel,
		                            .ar    = utcb.gs.ar,
		                            .limit = utcb.gs.limit,
		                            .base  = utcb.gs.base });
	}

	if (utcb.mtd & Nova::Mtd::TR) {
		state().tr.charge(Segment { .sel   = utcb.tr.sel,
		                            .ar    = utcb.tr.ar,
		                            .limit = utcb.tr.limit,
		                            .base  = utcb.tr.base });
	}

	if (utcb.mtd & Nova::Mtd::LDTR) {
		state().ldtr.charge(Segment { .sel   = utcb.ldtr.sel,
		                              .ar    = utcb.ldtr.ar,
		                              .limit = utcb.ldtr.limit,
		                              .base  = utcb.ldtr.base });
	}

	if (utcb.mtd & Nova::Mtd::GDTR) {
		state().gdtr.charge(Range { .limit = utcb.gdtr.limit,
		                            .base  = utcb.gdtr.base });
	}

	if (utcb.mtd & Nova::Mtd::IDTR) {
		state().idtr.charge(Range { .limit = utcb.idtr.limit,
		                            .base  = utcb.idtr.base });
	}

	if (utcb.mtd & Nova::Mtd::SYS) {
		state().sysenter_cs.charge(utcb.sysenter_cs);
		state().sysenter_sp.charge(utcb.sysenter_sp);
		state().sysenter_ip.charge(utcb.sysenter_ip);
	}

	if (utcb.mtd & Nova::Mtd::QUAL) {
		state().qual_primary.charge(utcb.qual[0]);
		state().qual_secondary.charge(utcb.qual[1]);
	}

	if (utcb.mtd & Nova::Mtd::CTRL) {
		state().ctrl_primary.charge(utcb.ctrl[0]);
		state().ctrl_secondary.charge(utcb.ctrl[1]);
	}

	if (utcb.mtd & Nova::Mtd::INJ) {
		state().inj_info.charge(utcb.inj_info);
		state().inj_error.charge(utcb.inj_error);
	}

	if (utcb.mtd & Nova::Mtd::STA) {
		state().intr_state.charge(utcb.intr_state);
		state().actv_state.charge(utcb.actv_state);
	}

	if (utcb.mtd & Nova::Mtd::TSC) {
		state().tsc.charge(utcb.tsc_val);
		state().tsc_offset.charge(utcb.tsc_off);
	}

	if (utcb.mtd & Nova::Mtd::TSC_AUX) {
		state().tsc_aux.charge(utcb.tsc_aux);
	}

	if (utcb.mtd & Nova::Mtd::EFER) {
		state().efer.charge(utcb.read_efer());
	}

	if (utcb.mtd & Nova::Mtd::PDPTE) {
		state().pdpte_0.charge(utcb.pdpte[0]);
		state().pdpte_1.charge(utcb.pdpte[1]);
		state().pdpte_2.charge(utcb.pdpte[2]);
		state().pdpte_3.charge(utcb.pdpte[3]);
	}

	if (utcb.mtd & Nova::Mtd::SYSCALL_SWAPGS) {
		state().star.charge(utcb.read_star());
		state().lstar.charge(utcb.read_lstar());
		state().cstar.charge(utcb.read_cstar());
		state().fmask.charge(utcb.read_fmask());
		state().kernel_gs_base.charge(utcb.read_kernel_gs_base());
	}

	if (utcb.mtd & Nova::Mtd::TPR) {
		state().tpr.charge(utcb.read_tpr());
		state().tpr_threshold.charge(utcb.read_tpr_threshold());
	}
}


void Nova_vcpu::_write_nova_state(Nova::Utcb &utcb)
{
	utcb.items = 0;
	utcb.mtd = 0;

	if (state().ax.charged() || state().cx.charged() ||
	    state().dx.charged() || state().bx.charged()) {
		utcb.mtd |= Nova::Mtd::ACDB;
		utcb.ax   = state().ax.value();
		utcb.cx   = state().cx.value();
		utcb.dx   = state().dx.value();
		utcb.bx   = state().bx.value();
	}

	if (state().bp.charged() || state().di.charged() || state().si.charged()) {
		utcb.mtd |= Nova::Mtd::EBSD;
		utcb.di   = state().di.value();
		utcb.si   = state().si.value();
		utcb.bp   = state().bp.value();
	}

	if (state().flags.charged()) {
		utcb.mtd |= Nova::Mtd::EFL;
		utcb.flags = state().flags.value();
	}

	if (state().sp.charged()) {
		utcb.mtd |= Nova::Mtd::ESP;
		utcb.sp = state().sp.value();
	}

	if (state().ip.charged()) {
		utcb.mtd |= Nova::Mtd::EIP;
		utcb.ip = state().ip.value();
		utcb.instr_len = state().ip_len.value();
	}

	if (state().dr7.charged()) {
		utcb.mtd |= Nova::Mtd::DR;
		utcb.dr7 = state().dr7.value();
	}

	if (state().r8 .charged() || state().r9 .charged() ||
	    state().r10.charged() || state().r11.charged() ||
	    state().r12.charged() || state().r13.charged() ||
	    state().r14.charged() || state().r15.charged()) {

		utcb.mtd |= Nova::Mtd::R8_R15;
		utcb.write_r8 (state().r8.value());
		utcb.write_r9 (state().r9.value());
		utcb.write_r10(state().r10.value());
		utcb.write_r11(state().r11.value());
		utcb.write_r12(state().r12.value());
		utcb.write_r13(state().r13.value());
		utcb.write_r14(state().r14.value());
		utcb.write_r15(state().r15.value());
	}

	if (state().cr0.charged() || state().cr2.charged() ||
	    state().cr3.charged() || state().cr4.charged()) {
		utcb.mtd |= Nova::Mtd::CR;
		utcb.cr0 = state().cr0.value();
		utcb.cr2 = state().cr2.value();
		utcb.cr3 = state().cr3.value();
		utcb.cr4 = state().cr4.value();
	}

	if (state().cs.charged() || state().ss.charged()) {
		utcb.mtd |= Nova::Mtd::CSSS;
		utcb.cs.sel   = state().cs.value().sel;
		utcb.cs.ar    = state().cs.value().ar;
		utcb.cs.limit = state().cs.value().limit;
		utcb.cs.base  = state().cs.value().base;

		utcb.ss.sel   = state().ss.value().sel;
		utcb.ss.ar    = state().ss.value().ar;
		utcb.ss.limit = state().ss.value().limit;
		utcb.ss.base  = state().ss.value().base;
	}

	if (state().es.charged() || state().ds.charged()) {
		utcb.mtd     |= Nova::Mtd::ESDS;
		utcb.es.sel   = state().es.value().sel;
		utcb.es.ar    = state().es.value().ar;
		utcb.es.limit = state().es.value().limit;
		utcb.es.base  = state().es.value().base;

		utcb.ds.sel   = state().ds.value().sel;
		utcb.ds.ar    = state().ds.value().ar;
		utcb.ds.limit = state().ds.value().limit;
		utcb.ds.base  = state().ds.value().base;
	}

	if (state().fs.charged() || state().gs.charged()) {
		utcb.mtd     |= Nova::Mtd::FSGS;
		utcb.fs.sel   = state().fs.value().sel;
		utcb.fs.ar    = state().fs.value().ar;
		utcb.fs.limit = state().fs.value().limit;
		utcb.fs.base  = state().fs.value().base;

		utcb.gs.sel   = state().gs.value().sel;
		utcb.gs.ar    = state().gs.value().ar;
		utcb.gs.limit = state().gs.value().limit;
		utcb.gs.base  = state().gs.value().base;
	}

	if (state().tr.charged()) {
		utcb.mtd     |= Nova::Mtd::TR;
		utcb.tr.sel   = state().tr.value().sel;
		utcb.tr.ar    = state().tr.value().ar;
		utcb.tr.limit = state().tr.value().limit;
		utcb.tr.base  = state().tr.value().base;
	}

	if (state().ldtr.charged()) {
		utcb.mtd       |= Nova::Mtd::LDTR;
		utcb.ldtr.sel   = state().ldtr.value().sel;
		utcb.ldtr.ar    = state().ldtr.value().ar;
		utcb.ldtr.limit = state().ldtr.value().limit;
		utcb.ldtr.base  = state().ldtr.value().base;
	}

	if (state().gdtr.charged()) {
		utcb.mtd       |= Nova::Mtd::GDTR;
		utcb.gdtr.limit = state().gdtr.value().limit;
		utcb.gdtr.base  = state().gdtr.value().base;
	}

	if (state().idtr.charged()) {
		utcb.mtd       |= Nova::Mtd::IDTR;
		utcb.idtr.limit = state().idtr.value().limit;
		utcb.idtr.base  = state().idtr.value().base;
	}

	if (state().sysenter_cs.charged() || state().sysenter_sp.charged() ||
	    state().sysenter_ip.charged()) {
		utcb.mtd        |= Nova::Mtd::SYS;
		utcb.sysenter_cs = state().sysenter_cs.value();
		utcb.sysenter_sp = state().sysenter_sp.value();
		utcb.sysenter_ip = state().sysenter_ip.value();
	}

	if (state().ctrl_primary.charged() || state().ctrl_secondary.charged()) {
		utcb.mtd     |= Nova::Mtd::CTRL;
		utcb.ctrl[0]  = state().ctrl_primary.value();
		utcb.ctrl[1]  = state().ctrl_secondary.value();
	}

	if (state().inj_info.charged() || state().inj_error.charged()) {
		utcb.mtd      |= Nova::Mtd::INJ;
		utcb.inj_info  = state().inj_info.value();
		utcb.inj_error = state().inj_error.value();
	}

	if (state().intr_state.charged() || state().actv_state.charged()) {
		utcb.mtd        |= Nova::Mtd::STA;
		utcb.intr_state  = state().intr_state.value();
		utcb.actv_state  = state().actv_state.value();
	}

	if (state().tsc.charged() || state().tsc_offset.charged()) {
		utcb.mtd     |= Nova::Mtd::TSC;
		utcb.tsc_val  = state().tsc.value();
		utcb.tsc_off  = state().tsc_offset.value();
	}

	if (state().tsc_aux.charged()) {
		utcb.mtd     |= Nova::Mtd::TSC_AUX;
		utcb.tsc_aux  = state().tsc_aux.value();
	}

	if (state().efer.charged()) {
		utcb.mtd |= Nova::Mtd::EFER;
		utcb.write_efer(state().efer.value());
	}

	if (state().pdpte_0.charged() || state().pdpte_1.charged() ||
	    state().pdpte_2.charged() || state().pdpte_3.charged()) {

		utcb.mtd |= Nova::Mtd::PDPTE;
		utcb.pdpte[0] = (Nova::mword_t)state().pdpte_0.value();
		utcb.pdpte[1] = (Nova::mword_t)state().pdpte_1.value();
		utcb.pdpte[2] = (Nova::mword_t)state().pdpte_2.value();
		utcb.pdpte[3] = (Nova::mword_t)state().pdpte_3.value();
	}

	if (state().star.charged() || state().lstar.charged() ||
	    state().cstar.charged() || state().fmask.charged() ||
	    state().kernel_gs_base.charged()) {

		utcb.mtd  |= Nova::Mtd::SYSCALL_SWAPGS;
		utcb.write_star (state().star.value());
		utcb.write_lstar(state().lstar.value());
		utcb.write_cstar(state().cstar.value());
		utcb.write_fmask(state().fmask.value());
		utcb.write_kernel_gs_base(state().kernel_gs_base.value());
	}

	if (state().tpr.charged() || state().tpr_threshold.charged()) {
		utcb.mtd |= Nova::Mtd::TPR;
		utcb.write_tpr(state().tpr.value());
		utcb.write_tpr_threshold(state().tpr_threshold.value());
	}

	if (state().fpu.charged()) {
		utcb.mtd |= Nova::Mtd::FPU;
		state().fpu.with_state([&] (Vcpu_state::Fpu::State const &fpu) {
			memcpy(utcb.fpu, &fpu, sizeof(fpu));
		});
	}
}


void Nova_vcpu::run()
{
	if (!_ep_handler) {
		/* not started yet - trigger startup of native vCPU */
		call<Rpc_startup>();
		return;
	}

	Thread * const current = Thread::myself();

	if (_dispatching == current) {
		_block = false;
		return;
	}

	if ((_ep_handler == current) && !_block)
		return;

	if (_ep_handler != current)
		_remote = RUN;

	Nova::ec_ctrl(Nova::EC_RECALL, _ec_sel());
	Nova::sm_ctrl(_sm_sel(), Nova::SEMAPHORE_UP);
}


/*
 * Do not touch the UTCB before _read_nova_state() and after
 * _write_nova_state(), particularly not by logging diagnostics.
 */
bool Nova_vcpu::_handle_exit(Nova::Utcb &utcb, uint16_t exit_reason)
{
	/* reset blocking state */
	bool const previous_blocked = _block;
	_block = true;

	/* NOVA specific exit reasons */
	enum { VM_EXIT_STARTUP = 0xfe, VM_EXIT_RECALL = 0xff };

	if (exit_reason == VM_EXIT_STARTUP)
		_ep_handler = Thread::myself();

	/* transform state from NOVA to Genode */
	if (exit_reason != VM_EXIT_RECALL || !previous_blocked)
		_read_nova_state(utcb, exit_reason);

	if (exit_reason == VM_EXIT_RECALL) {
		if (previous_blocked)
			state().exit_reason = exit_reason;

		/* consume potential multiple sem ups */
		Nova::sm_ctrl(_sm_sel(), Nova::SEMAPHORE_UP);
		Nova::sm_ctrl(_sm_sel(), Nova::SEMAPHORE_DOWNZERO);

		if (_remote == PAUSE) {
			_remote = NONE;
		} else {
			if (_remote == RUN) {
				_remote = NONE;
				if (!previous_blocked) {
					/* still running - reply without state transfer */
					_block = false;
					utcb.items = 0;
					utcb.mtd   = 0;
					return false;
				}
			}

			if (previous_blocked) {
				/* resume vCPU - with vCPU state update */
				_block = false;
				_write_nova_state(utcb);
				return false;
			}
		}
	}

	try {
		_dispatching = Thread::myself();
		/* call dispatch handler */
		_obj.dispatch(1);
		_dispatching = nullptr;
	} catch (...) {
		_dispatching = nullptr;
		throw;
	}

	if (_block) {
		/* block vCPU in kernel - no vCPU state update */
		utcb.items = 0;
		utcb.mtd   = 0;
		return true;
	}

	/* reply to NOVA and transfer vCPU state */
	_write_nova_state(utcb);
	return false;
}


void Nova_vcpu::_exit_entry(addr_t badge)
{
	Thread     &myself = *Thread::myself();
	Nova::Utcb &utcb   = *reinterpret_cast<Nova::Utcb *>(myself.utcb());

	uint16_t const exit_reason   { Badge(badge).exit_reason() };
	Vcpu_space::Id const vcpu_id { Badge(badge).vcpu_id() };

	try {
		_vcpu_space().apply<Nova_vcpu>(vcpu_id, [&] (Nova_vcpu &vcpu)
		{
			bool const block = vcpu._handle_exit(utcb, exit_reason);

			if (block) {
				Nova::reply(myself.stack_top(), vcpu._sm_sel());
			} else {
				Nova::reply(myself.stack_top());
			}
		});

	} catch (Vcpu_space::Unknown_id &) {

		/* somebody called us directly ? ... ignore/deny */
		utcb.items = 0;
		utcb.mtd   = 0;
		Nova::reply(myself.stack_top());
	}
}


void Nova_vcpu::pause()
{
	Thread * const current = Thread::myself();

	if (_dispatching == current) {
		/* current thread is already dispatching */
		if (_block)
			/* issue pause exit next time - fall through */
			_block = false;
		else {
			_block = true;
			return;
		}
	}

	if ((_ep_handler == current) && _block) {
		_remote = PAUSE;
		/* already blocked */
	}

	if (_ep_handler != current)
		_remote = PAUSE;

	if (!_ep_handler) {
		/* not started yet - let startup handler issue the recall */
		return;
	}

	Nova::ec_ctrl(Nova::EC_RECALL, _ec_sel());
	Nova::sm_ctrl(_sm_sel(), Nova::SEMAPHORE_UP);
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

	return vm.with_upgrade([&] () {
		return vm.call<Vm_session::Rpc_create_vcpu>(tep.cap()); });
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

void         Vm_connection::Vcpu::run()   {        static_cast<Nova_vcpu &>(_native_vcpu).run(); }
void         Vm_connection::Vcpu::pause() {        static_cast<Nova_vcpu &>(_native_vcpu).pause(); }
Vcpu_state & Vm_connection::Vcpu::state() { return static_cast<Nova_vcpu &>(_native_vcpu).state(); }


Vm_connection::Vcpu::Vcpu(Vm_connection &vm, Allocator &alloc,
                          Vcpu_handler_base &handler, Exit_config const &exit_config)
:
	_native_vcpu(*new (alloc) Nova_vcpu(vm._env, vm, alloc, handler, exit_config))
{ }

