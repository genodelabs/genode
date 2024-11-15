/*
 * \brief  Client-side VM session interface
 * \author Alexander Boettcher
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
#include <base/allocator.h>
#include <base/attached_dataspace.h>
#include <base/env.h>
#include <base/registry.h>
#include <base/sleep.h>

#include <vm_session/connection.h>
#include <vm_session/handler.h>

#include <cpu/vcpu_state.h>

#include <trace/timestamp.h>

#include <base/internal/native_thread.h>
#include <base/internal/native_utcb.h>
#include <base/internal/stack.h>
#include <base/internal/sel4.h>

/* seL4 includes */
#include <sel4_native_vcpu/sel4_native_vcpu.h>
#include <sel4/arch/vmenter.h>

using namespace Genode;

using Exit_config = Vm_connection::Exit_config;
using Call_with_state = Vm_connection::Call_with_state;

struct Sel4_vcpu;

struct Sel4_native_rpc : Rpc_client<Vm_session::Native_vcpu>, Noncopyable
{
	private:

		Capability<Vm_session::Native_vcpu> _create_vcpu(Vm_connection &vm,
		                                                 Thread_capability &cap)
		{
			return vm.create_vcpu(cap);
		}

	public:

		Sel4_vcpu &vcpu;

		Sel4_native_rpc(Vm_connection &vm, Thread_capability &cap,
		                Sel4_vcpu &vcpu)
		:
			Rpc_client<Vm_session::Native_vcpu>(_create_vcpu(vm, cap)),
			vcpu(vcpu)
		{ }
};

/******************************
 ** seL4 vCPU implementation **
 ******************************/

struct Sel4_vcpu : Genode::Thread, Noncopyable
{
	private:

		Vcpu_handler_base          &_vcpu_handler;
		Vcpu_handler<Sel4_vcpu>      _exit_handler;
		Vcpu_state                  _state __attribute__((aligned(0x10))) { };
		Semaphore                   _wake_up { 0 };
		Blockade                    _startup { };
		addr_t                      _recall { 0 };
		uint64_t                    _tsc_offset { 0 };
		Semaphore                   _state_ready { 0 };
		bool                        _dispatching { false };
		bool                        _extra_dispatch_up { false };
		void                       *_ep_handler    { nullptr };

		Constructible<Sel4_native_rpc>     _rpc   { };

		bool                        _show_error_unsupported_r { true };
		bool                        _show_error_unsupported_tpr { true };
		bool                        _show_error_unsupported_star { true };

		enum { EXIT_ON_HLT = 1U << 7, EXIT_ON_RDTSC = 1U << 12 };

		addr_t const                _vmcs_ctrl0 = EXIT_ON_HLT;

		enum { STACK_SIZE = 0x3000 };

		enum State {
			NONE = 0,
			PAUSE = 1,
			RUN = 2
		}                           _remote { NONE };
		Mutex                       _remote_mutex { };

		enum {
			VMEXIT_INVALID = 0x21,
			VMEXIT_STARTUP = 0xfe,
			VMEXIT_RECALL  = 0xff
		};

		enum {
			CR0_PE = 0, /* 1U << 0 - not needed in case of UG */
			CR0_CP = 1U << 1,
			CR0_NE = 1U << 5,
			CR0_NM = 1U << 29,
			CR0_CD = 1U << 30,
			CR0_PG = 0, /* 1U << 31 - not needed in case of UG */

			CR4_VMX        = 1 << 13,
		};

		addr_t const cr0_mask = CR0_PE | CR0_CP | CR0_NE | CR0_NM | CR0_CD | CR0_PG;
		addr_t const cr0_set  = 0;
		addr_t const cr4_mask = CR4_VMX;
		addr_t const cr4_set  = CR4_VMX;

		seL4_VCPUContext _recent_gpr { };

		void entry() override
		{
			/* trigger that thread is up */
			_startup.wakeup();

			/* wait until vcpu is assigned to us */
			_wake_up.down();

			/* get selector to read/write VMCS */
			addr_t const service = _stack->utcb().ep_sel();
			/* get selector to call back a vCPU into VMM */
			_recall = _stack->utcb().lock_sel();

			Vcpu_state &state = _state;
			state.discharge();

			/* wait for first user resume() */
			_wake_up.down();

			{
				Mutex::Guard guard(_remote_mutex);
				_remote = NONE;
			}

			/* initial startup VM exit to get valid VM state */
			state.exit_reason = VMEXIT_STARTUP;
			_read_sel4_state(service, state);

			_state_ready.up();
			Signal_transmitter(_exit_handler.signal_cap()).submit();

			_exit_handler.ready_semaphore().down();
			_wake_up.down();

			State local_state { NONE };

			_write_vmcs(service, Vmcs::CR0_MASK, cr0_mask);
			_write_vmcs(service, Vmcs::CR4_MASK, cr4_mask);

			while (true) {
				/* read in requested state from remote threads */
				{
					Mutex::Guard guard(_remote_mutex);

					local_state     = _remote;
					_remote          = NONE;

					if (local_state == PAUSE) {
						_write_sel4_state(service, state);

						seL4_Word badge = 0;
						/* consume spurious notification - XXX better way ? */
						seL4_SetMR(0, state.ip.value());
						seL4_SetMR(1, _vmcs_ctrl0 | state.ctrl_primary.value());
						seL4_SetMR(2, state.inj_info.value() & ~0x3000U);
						if (seL4_VMEnter(&badge) == SEL4_VMENTER_RESULT_FAULT)
							Genode::error("invalid state ahead ", badge);
					}
				}

				if (local_state == NONE) {
					_wake_up.down();
					continue;
				}

				if (local_state == PAUSE) {

					state.discharge();

					state.exit_reason = VMEXIT_RECALL;

					_read_sel4_state(service, state);

					_state_ready.up();

					if (_extra_dispatch_up) {
						_extra_dispatch_up = false;
						_exit_handler.ready_semaphore().down();
					}

					continue;
				}

				if (local_state != RUN) {
					Genode::error("unknown vcpu state ", (int)local_state);
					while (true) { _remote_mutex.acquire(); }
				}

				_write_sel4_state(service, state);

				seL4_SetMR(0, state.ip.value());
				seL4_SetMR(1, _vmcs_ctrl0 | state.ctrl_primary.value());
				seL4_SetMR(2, state.inj_info.value() & ~0x3000U);

				seL4_Word badge = 0;
				seL4_Word res = seL4_VMEnter(&badge);

				state.discharge();

				/*
				 * If a VMEXIT_RECALL is dispatched here, it comes from a
				 * pause request sent by an already running asynchronous signal
				 * handler.
				 * In that case, don't dispatch an extra exit signal.
				 */
				bool skip_dispatch = false;

				if (res != SEL4_VMENTER_RESULT_FAULT) {
					state.exit_reason = VMEXIT_RECALL;
					skip_dispatch = true;
				}
				else
					state.exit_reason = (unsigned)seL4_GetMR(SEL4_VMENTER_FAULT_REASON_MR);

				_read_sel4_state(service, state);

				if (res != SEL4_VMENTER_RESULT_FAULT) {
					Mutex::Guard guard(_remote_mutex);
					if (_remote == PAUSE) {
						_remote = NONE;
						_wake_up.down();
					}
				}
				_state_ready.up();

				if (skip_dispatch)
					continue;
				/* notify VM handler */
				Genode::Signal_transmitter(_exit_handler.signal_cap()).submit();

				/*
				 * Wait until VM handler is really really done,
				 * otherwise we lose state.
				 */
				_exit_handler.ready_semaphore().down();
			}
		}

		enum Vmcs {
			IRQ_WINDOW = 1U << 2,

			CR0        = 0x6800,
			CR0_MASK   = 0x6000,
			CR0_SHADOW = 0x6004,

			CR3 = 0x6802,

			CR4        = 0x6804,
			CR4_MASK   = 0x6002,
			CR4_SHADOW = 0x6006,

			DR7        = 0x681a,

			RFLAGS = 0x6820,

			RSP = 0x681c,
			RIP = 0x681e,

			EFER = 0x2806,

			CTRL_0 = 0x4002,
			CTRL_1 = 0x401e,

			CS_SEL   = 0x0802,
			CS_LIMIT = 0x4802,
			CS_AR    = 0x4816,
			CS_BASE  = 0x6808,

			SS_SEL   = 0x0804,
			SS_LIMIT = 0x4804,
			SS_AR    = 0x4818,
			SS_BASE  = 0x680a,

			ES_SEL   = 0x0800,
			ES_LIMIT = 0x4800,
			ES_AR    = 0x4814,
			ES_BASE  = 0x6806,

			DS_SEL   = 0x0806,
			DS_LIMIT = 0x4806,
			DS_AR    = 0x481a,
			DS_BASE  = 0x680c,

			FS_SEL   = 0x0808,
			FS_LIMIT = 0x4808,
			FS_AR    = 0x481c,
			FS_BASE  = 0x680e,

			GS_SEL   = 0x080a,
			GS_LIMIT = 0x480a,
			GS_AR    = 0x481e,
			GS_BASE  = 0x6810,

			LDTR_SEL   = 0x080c,
			LDTR_LIMIT = 0x480c,
			LDTR_AR    = 0x4820,
			LDTR_BASE  = 0x6812,

			TR_SEL   = 0x080e,
			TR_LIMIT = 0x480e,
			TR_AR    = 0x4822,
			TR_BASE  = 0x6814,

			IDTR_LIMIT = 0x4812,
			IDTR_BASE  = 0x6818,

			GDTR_LIMIT = 0x4810,
			GDTR_BASE  = 0x6816,

			PDPTE_0 = 0x280a,
			PDPTE_1 = 0x280c,
			PDPTE_2 = 0x280e,
			PDPTE_3 = 0x2810,

			SYSENTER_CS = 0x482a,
			SYSENTER_SP = 0x6824,
			SYSENTER_IP = 0x6826,

			STATE_INTR  = 0x4824,
			STATE_ACTV  = 0x4826,

			INTR_INFO  = 0x4016,
			INTR_ERROR = 0x4018,
			ENTRY_INST_LEN = 0x401a,

			IDT_INFO = 0x4408,
			IDT_ERROR = 0x440a,
			EXIT_INST_LEN = 0x440c,

			TSC_OFF_LO = 0x2010,
			TSC_OFF_HI = 0x2011,
		};

		void _write_vmcs(seL4_X86_VCPU const service, enum Vmcs const field,
		                 seL4_Word const value)
		{
			seL4_X86_VCPU_WriteVMCS_t res;
			res = seL4_X86_VCPU_WriteVMCS(service, field, value);
			if (res.error != seL4_NoError)
				Genode::error("field ", Hex(field), " - ", res.error,
				              " ", res.written);
		}

		/*
		 * Convert to Intel format comprising 32 bits.
		 */
		addr_t _convert_ar(addr_t value) {
			return ((value << 4) & 0x1f000) | (value & 0xff); }

		/*
		 * Convert to AMD (and Genode) format comprising 16 bits.
		 */
		uint16_t _convert_ar_16(addr_t value) {
			return (uint16_t)(((value & 0x1f000) >> 4) | (value & 0xff)); }

		void _write_sel4_state(seL4_X86_VCPU const service, Vcpu_state &state)
		{
			if (state.ax.charged()) _recent_gpr.eax = state.ax.value();
			if (state.bx.charged()) _recent_gpr.ebx = state.bx.value();
			if (state.cx.charged()) _recent_gpr.ecx = state.cx.value();
			if (state.dx.charged()) _recent_gpr.edx = state.dx.value();
			if (state.si.charged()) _recent_gpr.esi = state.si.value();
			if (state.di.charged()) _recent_gpr.edi = state.di.value();
			if (state.bp.charged()) _recent_gpr.ebp = state.bp.value();

			if (state.ax.charged() || state.cx.charged() ||
			    state.dx.charged() || state.bx.charged() ||
			    state.bp.charged() || state.di.charged() ||
			    state.si.charged())
			{
				seL4_Error res = seL4_X86_VCPU_WriteRegisters(service,
				                                              &_recent_gpr);
				if (res != seL4_NoError)
					Genode::error("setting general purpose registers failed ",
					              (int)res);
			}

			if (state.r8.charged()  || state.r9.charged() ||
			    state.r10.charged() || state.r11.charged() ||
			    state.r12.charged() || state.r13.charged() ||
			    state.r14.charged() || state.r15.charged())
			{
				if (_show_error_unsupported_r)
				{
					_show_error_unsupported_r = false;
					Genode::error("registers r8-15 not supported by seL4");
				}
			}

			if (state.tsc.charged() || state.tsc_offset.charged())
			{
				_tsc_offset += state.tsc_offset.value();
				/* not supported by seL4 */
#if 0
				_write_vmcs(service, Vmcs::TSC_OFF_LO,  _tsc_offset & 0xffffffffu);
				_write_vmcs(service, Vmcs::TSC_OFF_HI, (_tsc_offset >> 32) & 0xffffffffu);
#endif
			}

			if (state.star.charged() || state.lstar.charged() || state.cstar.charged() ||
			    state.fmask.charged() || state.kernel_gs_base.charged())
			{
				if (_show_error_unsupported_star) {
					_show_error_unsupported_star = false;
					Genode::error("star, lstar, cstar, fmask, gs_base not supported by seL4");
				}
			}

			if (state.tpr.charged() || state.tpr_threshold.charged())
			{
				if (_show_error_unsupported_tpr)
				{
					_show_error_unsupported_tpr = false;
					Genode::error("tpr* not supported by seL4");
				}
			}

			if (state.dr7.charged())
				_write_vmcs(service, Vmcs::DR7, state.dr7.value());

			if (state.cr0.charged()) {
				_write_vmcs(service, Vmcs::CR0, cr0_set | (~cr0_mask & state.cr0.value()));
				_write_vmcs(service, Vmcs::CR0_SHADOW, state.cr0.value());
			}

			/* not supported on seL4 - state.cr2.charged() */

			if (state.cr3.charged())
				_write_vmcs(service, Vmcs::CR3, state.cr3.value());

			if (state.cr4.charged()) {
				_write_vmcs(service, Vmcs::CR4, cr4_set | (~cr4_mask & state.cr4.value()));
				_write_vmcs(service, Vmcs::CR4_SHADOW, state.cr4.value());
			}

			if (state.inj_info.charged()) {
				addr_t ctrl_0 = state.ctrl_primary.charged() ?
				                state.ctrl_primary.value() :
				                _read_vmcs(service, Vmcs::CTRL_0);

				if (state.inj_info.value() & 0x2000)
					Genode::warning("inj_info for NMI not supported");

				if (state.inj_info.value() & 0x1000)
					ctrl_0 |= Vmcs::IRQ_WINDOW;
				else
					ctrl_0 &= ~Vmcs::IRQ_WINDOW;

				state.ctrl_primary.charge((uint32_t)ctrl_0);
			}

			if (state.inj_error.charged())
				_write_vmcs(service, Vmcs::INTR_ERROR, state.inj_error.value());

			if (state.flags.charged())
				_write_vmcs(service, Vmcs::RFLAGS, state.flags.value());

			if (state.sp.charged())
				_write_vmcs(service, Vmcs::RSP, state.sp.value());

			if (state.ip.charged())
				_write_vmcs(service, Vmcs::RIP, state.ip.value());

			if (state.ip_len.charged())
				_write_vmcs(service, Vmcs::ENTRY_INST_LEN, state.ip_len.value());

			if (state.efer.charged())
				_write_vmcs(service, Vmcs::EFER, state.efer.value());

			/* state.ctrl_primary.charged() update on vmenter - see above */

			if (state.ctrl_secondary.charged())
				_write_vmcs(service, Vmcs::CTRL_1, state.ctrl_secondary.value());

			if (state.intr_state.charged())
				_write_vmcs(service, Vmcs::STATE_INTR, state.intr_state.value());

			if (state.actv_state.charged())
				_write_vmcs(service, Vmcs::STATE_ACTV, state.actv_state.value());

			if (state.cs.charged()) {
				_write_vmcs(service, Vmcs::CS_SEL,   state.cs.value().sel);
				_write_vmcs(service, Vmcs::CS_LIMIT, state.cs.value().limit);
				_write_vmcs(service, Vmcs::CS_AR, _convert_ar(state.cs.value().ar));
				_write_vmcs(service, Vmcs::CS_BASE,  state.cs.value().base);
			}

			if (state.ss.charged()) {
				_write_vmcs(service, Vmcs::SS_SEL,   state.ss.value().sel);
				_write_vmcs(service, Vmcs::SS_LIMIT, state.ss.value().limit);
				_write_vmcs(service, Vmcs::SS_AR, _convert_ar(state.ss.value().ar));
				_write_vmcs(service, Vmcs::SS_BASE,  state.ss.value().base);
			}

			if (state.es.charged()) {
				_write_vmcs(service, Vmcs::ES_SEL,   state.es.value().sel);
				_write_vmcs(service, Vmcs::ES_LIMIT, state.es.value().limit);
				_write_vmcs(service, Vmcs::ES_AR, _convert_ar(state.es.value().ar));
				_write_vmcs(service, Vmcs::ES_BASE,  state.es.value().base);
			}

			if (state.ds.charged()) {
				_write_vmcs(service, Vmcs::DS_SEL,   state.ds.value().sel);
				_write_vmcs(service, Vmcs::DS_LIMIT, state.ds.value().limit);
				_write_vmcs(service, Vmcs::DS_AR, _convert_ar(state.ds.value().ar));
				_write_vmcs(service, Vmcs::DS_BASE,  state.ds.value().base);
			}

			if (state.fs.charged()) {
				_write_vmcs(service, Vmcs::FS_SEL,   state.fs.value().sel);
				_write_vmcs(service, Vmcs::FS_LIMIT, state.fs.value().limit);
				_write_vmcs(service, Vmcs::FS_AR, _convert_ar(state.fs.value().ar));
				_write_vmcs(service, Vmcs::FS_BASE,  state.fs.value().base);
			}

			if (state.gs.charged()) {
				_write_vmcs(service, Vmcs::GS_SEL,   state.gs.value().sel);
				_write_vmcs(service, Vmcs::GS_LIMIT, state.gs.value().limit);
				_write_vmcs(service, Vmcs::GS_AR, _convert_ar(state.gs.value().ar));
				_write_vmcs(service, Vmcs::GS_BASE,  state.gs.value().base);
			}

			if (state.tr.charged()) {
				_write_vmcs(service, Vmcs::TR_SEL,   state.tr.value().sel);
				_write_vmcs(service, Vmcs::TR_LIMIT, state.tr.value().limit);
				_write_vmcs(service, Vmcs::TR_AR, _convert_ar(state.tr.value().ar));
				_write_vmcs(service, Vmcs::TR_BASE,  state.tr.value().base);
			}

			if (state.ldtr.charged()) {
				_write_vmcs(service, Vmcs::LDTR_SEL,   state.ldtr.value().sel);
				_write_vmcs(service, Vmcs::LDTR_LIMIT, state.ldtr.value().limit);
				_write_vmcs(service, Vmcs::LDTR_AR, _convert_ar(state.ldtr.value().ar));
				_write_vmcs(service, Vmcs::LDTR_BASE,  state.ldtr.value().base);
			}

			if (state.idtr.charged()) {
				_write_vmcs(service, Vmcs::IDTR_BASE,  state.idtr.value().base);
				_write_vmcs(service, Vmcs::IDTR_LIMIT, state.idtr.value().limit);
			}

			if (state.gdtr.charged()) {
				_write_vmcs(service, Vmcs::GDTR_BASE,  state.gdtr.value().base);
				_write_vmcs(service, Vmcs::GDTR_LIMIT, state.gdtr.value().limit);
			}

			if (state.pdpte_0.charged())
				_write_vmcs(service, Vmcs::PDPTE_0, (addr_t)state.pdpte_0.value());

			if (state.pdpte_1.charged())
				_write_vmcs(service, Vmcs::PDPTE_1, (addr_t)state.pdpte_1.value());

			if (state.pdpte_2.charged())
				_write_vmcs(service, Vmcs::PDPTE_2, (addr_t)state.pdpte_2.value());

			if (state.pdpte_3.charged())
				_write_vmcs(service, Vmcs::PDPTE_3, (addr_t)state.pdpte_3.value());

			if (state.sysenter_cs.charged())
				_write_vmcs(service, Vmcs::SYSENTER_CS, state.sysenter_cs.value());

			if (state.sysenter_sp.charged())
				_write_vmcs(service, Vmcs::SYSENTER_SP, state.sysenter_sp.value());

			if (state.sysenter_ip.charged())
				_write_vmcs(service, Vmcs::SYSENTER_IP, state.sysenter_ip.value());
		}

		seL4_Word _read_vmcs(seL4_X86_VCPU const service, enum Vmcs const field)
		{
			seL4_X86_VCPU_ReadVMCS_t res;
			res = seL4_X86_VCPU_ReadVMCS(service, field);
			if (res.error != seL4_NoError)
				Genode::error("field ", Hex(field), " - ", res.error);
			return res.value;
		}

		template <typename T>
		T _read_vmcsX(seL4_X86_VCPU const service, enum Vmcs const field)
		{
			seL4_X86_VCPU_ReadVMCS_t res;
			res = seL4_X86_VCPU_ReadVMCS(service, field);
			if (res.error != seL4_NoError)
				Genode::error("field ", Hex(field), " - ", res.error);
			return (T)(res.value);
		}

		uint16_t _read_vmcs_16(seL4_X86_VCPU const service, enum Vmcs const field) {
			return _read_vmcsX<uint16_t>(service, field); }

		uint32_t _read_vmcs_32(seL4_X86_VCPU const service, enum Vmcs const field) {
			return _read_vmcsX<uint32_t>(service, field); }

		void _read_sel4_state(seL4_X86_VCPU const service, Vcpu_state &state)
		{
			state.ip.charge(seL4_GetMR(SEL4_VMENTER_CALL_EIP_MR));
			state.ctrl_primary.charge((uint32_t)seL4_GetMR(SEL4_VMENTER_CALL_CONTROL_PPC_MR));

			state.ip_len        .charge(seL4_GetMR(SEL4_VMENTER_FAULT_INSTRUCTION_LEN_MR));
			state.qual_primary  .charge(seL4_GetMR(SEL4_VMENTER_FAULT_QUALIFICATION_MR));
			state.qual_secondary.charge(seL4_GetMR(SEL4_VMENTER_FAULT_GUEST_PHYSICAL_MR));

			state.flags     .charge(seL4_GetMR(SEL4_VMENTER_FAULT_RFLAGS_MR));
			state.intr_state.charge((uint32_t)seL4_GetMR(SEL4_VMENTER_FAULT_GUEST_INT_MR));
			state.cr3       .charge(seL4_GetMR(SEL4_VMENTER_FAULT_CR3_MR));

			state.ax.charge(seL4_GetMR(SEL4_VMENTER_FAULT_EAX));
			state.bx.charge(seL4_GetMR(SEL4_VMENTER_FAULT_EBX));
			state.cx.charge(seL4_GetMR(SEL4_VMENTER_FAULT_ECX));
			state.dx.charge(seL4_GetMR(SEL4_VMENTER_FAULT_EDX));
			state.si.charge(seL4_GetMR(SEL4_VMENTER_FAULT_ESI));
			state.di.charge(seL4_GetMR(SEL4_VMENTER_FAULT_EDI));
			state.bp.charge(seL4_GetMR(SEL4_VMENTER_FAULT_EBP));

			_recent_gpr.eax = state.ax.value();
			_recent_gpr.ebx = state.bx.value();
			_recent_gpr.ecx = state.cx.value();
			_recent_gpr.edx = state.dx.value();
			_recent_gpr.esi = state.si.value();
			_recent_gpr.edi = state.di.value();
			_recent_gpr.ebp = state.bp.value();

			state.sp .charge(_read_vmcs(service, Vmcs::RSP));
			state.dr7.charge(_read_vmcs(service, Vmcs::DR7));

			/* r8 - r15 not supported on seL4 */

			{
				addr_t const cr0        = _read_vmcs(service, Vmcs::CR0);
				addr_t const cr0_shadow = _read_vmcs(service, Vmcs::CR0_SHADOW);
				state.cr0.charge((cr0 & ~cr0_mask) | (cr0_shadow & cr0_mask));

				if (state.cr0.value() != cr0_shadow)
					_write_vmcs(service, Vmcs::CR0_SHADOW, state.cr0.value());
			}

			/* cr2 not supported on seL4 */
			state.cr2.charge(state.cr2.value());

			{
				addr_t const cr4        = _read_vmcs(service, Vmcs::CR4);
				addr_t const cr4_shadow = _read_vmcs(service, Vmcs::CR4_SHADOW);
				state.cr4.charge((cr4 & ~cr4_mask) | (cr4_shadow & cr4_mask));

				if (state.cr4.value() != cr4_shadow)
					_write_vmcs(service, Vmcs::CR4_SHADOW, state.cr4.value());
			}

			using Segment = Genode::Vcpu_state::Segment;
			using Range   = Genode::Vcpu_state::Range;

			state.cs.charge(Segment{_read_vmcs_16(service, Vmcs::CS_SEL),
			                       _convert_ar_16(_read_vmcs(service, Vmcs::CS_AR)),
			                       _read_vmcs_32(service, Vmcs::CS_LIMIT),
			                       _read_vmcs(service, Vmcs::CS_BASE)});

			state.ss.charge(Segment{_read_vmcs_16(service, Vmcs::SS_SEL),
			                       _convert_ar_16(_read_vmcs(service, Vmcs::SS_AR)),
			                       _read_vmcs_32(service, Vmcs::SS_LIMIT),
			                       _read_vmcs(service, Vmcs::SS_BASE)});

			state.es.charge(Segment{_read_vmcs_16(service, Vmcs::ES_SEL),
			                       _convert_ar_16(_read_vmcs(service, Vmcs::ES_AR)),
			                       _read_vmcs_32(service, Vmcs::ES_LIMIT),
			                       _read_vmcs(service, Vmcs::ES_BASE)});

			state.ds.charge(Segment{_read_vmcs_16(service, Vmcs::DS_SEL),
			                       _convert_ar_16(_read_vmcs(service, Vmcs::DS_AR)),
			                       _read_vmcs_32(service, Vmcs::DS_LIMIT),
			                       _read_vmcs(service, Vmcs::DS_BASE)});

			state.fs.charge(Segment{_read_vmcs_16(service, Vmcs::FS_SEL),
			                       _convert_ar_16(_read_vmcs(service, Vmcs::FS_AR)),
			                       _read_vmcs_32(service, Vmcs::FS_LIMIT),
			                       _read_vmcs(service, Vmcs::FS_BASE)});

			state.gs.charge(Segment{_read_vmcs_16(service, Vmcs::GS_SEL),
			                       _convert_ar_16(_read_vmcs(service, Vmcs::GS_AR)),
			                       _read_vmcs_32(service, Vmcs::GS_LIMIT),
			                       _read_vmcs(service, Vmcs::GS_BASE)});

			state.tr.charge(Segment{_read_vmcs_16(service, Vmcs::TR_SEL),
			                       _convert_ar_16(_read_vmcs(service, Vmcs::TR_AR)),
			                       _read_vmcs_32(service, Vmcs::TR_LIMIT),
			                       _read_vmcs(service, Vmcs::TR_BASE)});

			state.ldtr.charge(Segment{_read_vmcs_16(service, Vmcs::LDTR_SEL),
			                         _convert_ar_16(_read_vmcs(service, Vmcs::LDTR_AR)),
			                         _read_vmcs_32(service, Vmcs::LDTR_LIMIT),
			                         _read_vmcs(service, Vmcs::LDTR_BASE)});

			state.idtr.charge(Range{ .limit = _read_vmcs_32(service, Vmcs::IDTR_LIMIT),
			                         .base  = _read_vmcs(service, Vmcs::IDTR_BASE) });

			state.gdtr.charge(Range{ .limit = _read_vmcs_32(service, Vmcs::GDTR_LIMIT),
			                         .base  = _read_vmcs(service, Vmcs::GDTR_BASE) });

			state.sysenter_cs.charge(_read_vmcs(service, Vmcs::SYSENTER_CS));
			state.sysenter_sp.charge(_read_vmcs(service, Vmcs::SYSENTER_SP));
			state.sysenter_ip.charge(_read_vmcs(service, Vmcs::SYSENTER_IP));

			/* no support by seL4 to read this value */
			state.ctrl_secondary.charge(state.ctrl_secondary.value());
			//state.ctrl_secondary.charge(_read_vmcs(service, Vmcs::CTRL_1));

			if (state.exit_reason == VMEXIT_INVALID ||
			    state.exit_reason == VMEXIT_RECALL)
			{
				state.inj_info .charge((uint32_t)_read_vmcs(service, Vmcs::INTR_INFO));
				state.inj_error.charge((uint32_t)_read_vmcs(service, Vmcs::INTR_ERROR));
			} else {
				state.inj_info .charge((uint32_t)_read_vmcs(service, Vmcs::IDT_INFO));
				state.inj_error.charge((uint32_t)_read_vmcs(service, Vmcs::IDT_ERROR));
			}

			state.intr_state.charge((uint32_t)_read_vmcs(service, Vmcs::STATE_INTR));
			state.actv_state.charge((uint32_t)_read_vmcs(service, Vmcs::STATE_ACTV));

			state.pdpte_0.charge(_read_vmcs(service, Vmcs::PDPTE_0));
			state.pdpte_1.charge(_read_vmcs(service, Vmcs::PDPTE_1));
			state.pdpte_2.charge(_read_vmcs(service, Vmcs::PDPTE_2));
			state.pdpte_3.charge(_read_vmcs(service, Vmcs::PDPTE_3));

			/* tsc and tsc_offset not supported by seL4 */
			state.tsc.charge(Trace::timestamp());
			state.tsc_offset.charge(_tsc_offset);

			state.efer.charge(_read_vmcs(service, Vmcs::EFER));

			/* XXX star, lstar, cstar, fmask, kernel_gs_base not supported by seL4 */

			/* XXX tpr and tpr_threshold not supported by seL4 */
		}

		Affinity::Location _location(Vcpu_handler_base &handler) const
		{
			Thread * ep = reinterpret_cast<Thread *>(&handler.rpc_ep());
			return ep->affinity();
		}

		void _wrapper_dispatch()
		{
			_dispatching = true;
			_vcpu_handler.dispatch(1);
			_dispatching = false;
		}

	public:

		Sel4_vcpu(Env &env, Vm_connection &vm,
		          Vcpu_handler_base &handler, Exit_config const &)
		:
			Thread(env, "vcpu_thread", STACK_SIZE, _location(handler),
			       Weight(), env.cpu()),
			_vcpu_handler(handler),
	 	 	_exit_handler(handler.ep(), *this, &Sel4_vcpu::_wrapper_dispatch)
		{
			Thread::start();

			/* wait until thread is alive, e.g. Thread::cap() is valid */
			_startup.block();
			_ep_handler = reinterpret_cast<Thread *>(&handler.rpc_ep());

			_rpc.construct(vm, this->cap(), *this);

			/* signal about finished vCPU assignment */
			_wake_up.up();
		}

		const Sel4_vcpu& operator=(const Sel4_vcpu &) = delete;
		Sel4_vcpu(const Sel4_vcpu&) = delete;

		void resume()
		{
			Mutex::Guard guard(_remote_mutex);

			if (_remote == RUN || _remote == PAUSE)
				return;

			_remote = RUN;
			_wake_up.up();
		}

		void with_state(Call_with_state &cw)
		{
			if (!_dispatching) {
				if (Thread::myself() != _ep_handler) {
					error("vCPU state requested outside of vcpu_handler EP");
					sleep_forever();
				}

				_remote_mutex.acquire();

				/* Trigger pause exit */
				_remote = PAUSE;
				seL4_Signal(_recall);
				_wake_up.up();

				_remote_mutex.release();
				_state_ready.down();

				/*
				 * We're in the async dispatch, yet processing a non-pause exit.
				 * Signal that we have to wrap the dispatch loop around.
				 */
				if (_state.exit_reason != VMEXIT_RECALL) {
					_extra_dispatch_up = true;
				}
			} else {
				_state_ready.down();
			}

			if (cw.call_with_state(_state)
			    || _extra_dispatch_up)
				resume();

			/*
			 * The regular exit was handled by the asynchronous dispatch handler
			 * triggered by the pause request.
			 *
			 * Fake finishing the exit dispatch so that the vCPU loop
			 * processes the asynchronously dispatched exit and provides
			 * the VMEXIT_RECALL to the already pending dispatch function
			 * for the exit code.
			 */
			if (!_dispatching && _extra_dispatch_up)
				_exit_handler.ready_semaphore().up();
		}

		Sel4_native_rpc * rpc()   { return &*_rpc; }
};

/**************
 ** vCPU API **
 **************/

void Vm_connection::Vcpu::_with_state(Call_with_state &cw) { static_cast<Sel4_native_rpc &>(_native_vcpu).vcpu.with_state(cw); }


Vm_connection::Vcpu::Vcpu(Vm_connection &vm, Allocator &alloc,
                          Vcpu_handler_base &handler, Exit_config const &exit_config)
:
	_native_vcpu(*((new (alloc) Sel4_vcpu(vm._env, vm, handler, exit_config))->rpc()))
{
	static_cast<Sel4_native_rpc &>(_native_vcpu).vcpu.resume();
}
