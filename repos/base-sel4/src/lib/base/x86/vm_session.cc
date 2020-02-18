/*
 * \brief  Client-side VM session interface
 * \author Alexander Boettcher
 * \date   2018-08-27
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <base/allocator.h>
#include <base/env.h>
#include <base/registry.h>
#include <vm_session/client.h>

#include <cpu/vm_state.h>

#include <trace/timestamp.h>

#include <base/internal/native_thread.h>
#include <base/internal/native_utcb.h>

#include <sel4/sel4.h>
#include <sel4/arch/vmenter.h>

#include <base/internal/stack.h>
using namespace Genode;

struct Vcpu;

static Genode::Registry<Genode::Registered<Vcpu> > vcpus;

struct Vcpu : Genode::Thread
{
	private:

		Signal_context_capability  &_signal;
		Semaphore                   _wake_up { 0 };
		Semaphore                  &_handler_ready;
		Allocator                  &_alloc;
		Blockade                    _startup { };
		Vm_session_client::Vcpu_id  _id {};
		addr_t                      _state { 0 };
		addr_t                      _recall { 0 };
		uint64_t                    _tsc_offset { 0 };

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

			Vm_state &state = *reinterpret_cast<Vm_state *>(_state);
			state = Vm_state {};

			/* wait for first user resume() */
			_wake_up.down();

			{
				Mutex::Guard guard(_remote_mutex);
				_remote = NONE;
			}

			/* initial startup VM exit to get valid VM state */
			state.exit_reason = VMEXIT_STARTUP;
			_read_sel4_state(service, state);

			Genode::Signal_transmitter(_signal).submit();

			_handler_ready.down();
			_wake_up.down();

			State local_state { NONE };

			_write_vmcs(service, Vmcs::CR0_MASK, cr0_mask);
			_write_vmcs(service, Vmcs::CR4_MASK, cr4_mask);

			while (true) {
				/* read in requested state from remote threads */
				{
					Mutex::Guard guard(_remote_mutex);

					local_state      = _remote;
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

					state = Vm_state {};

					state.exit_reason = VMEXIT_RECALL;

					_read_sel4_state(service, state);

					/* notify VM handler */
					Genode::Signal_transmitter(_signal).submit();

					/*
					 * Wait until VM handler is really really done,
					 * otherwise we lose state.
					 */
					_handler_ready.down();
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

				state = Vm_state {};

				if (res != SEL4_VMENTER_RESULT_FAULT)
					state.exit_reason = VMEXIT_RECALL;
				else
					state.exit_reason = seL4_GetMR(SEL4_VMENTER_FAULT_REASON_MR);

				_read_sel4_state(service, state);

				if (res != SEL4_VMENTER_RESULT_FAULT) {
					Mutex::Guard guard(_remote_mutex);
					if (_remote == PAUSE) {
						_remote = NONE;
						_wake_up.down();
					}
				}

				/* notify VM handler */
				Genode::Signal_transmitter(_signal).submit();

				/*
				 * Wait until VM handler is really really done,
				 * otherwise we lose state.
				 */
				_handler_ready.down();
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
			return ((value & 0x1f000) >> 4) | (value & 0xff); }

		void _write_sel4_state(seL4_X86_VCPU const service, Vm_state &state)
		{
			if (state.ax.valid()) _recent_gpr.eax = state.ax.value();
			if (state.bx.valid()) _recent_gpr.ebx = state.bx.value();
			if (state.cx.valid()) _recent_gpr.ecx = state.cx.value();
			if (state.dx.valid()) _recent_gpr.edx = state.dx.value();
			if (state.si.valid()) _recent_gpr.esi = state.si.value();
			if (state.di.valid()) _recent_gpr.edi = state.di.value();
			if (state.bp.valid()) _recent_gpr.ebp = state.bp.value();

			if (state.ax.valid() || state.cx.valid() ||
			    state.dx.valid() || state.bx.valid() ||
			    state.bp.valid() || state.di.valid() ||
			    state.si.valid())
			{
				seL4_Error res = seL4_X86_VCPU_WriteRegisters(service,
				                                              &_recent_gpr);
				if (res != seL4_NoError)
					Genode::error("setting general purpose registers failed ",
					              (int)res);
			}

			if (state.r8.valid()  || state.r9.valid() ||
			    state.r10.valid() || state.r11.valid() ||
			    state.r12.valid() || state.r13.valid() ||
			    state.r14.valid() || state.r15.valid())
			{
				if (_show_error_unsupported_r)
				{
					_show_error_unsupported_r = false;
					Genode::error("registers r8-15 not supported by seL4");
				}
			}

			if (state.tsc.valid() || state.tsc_offset.valid())
			{
				_tsc_offset += state.tsc_offset.value();
				/* not supported by seL4 */
#if 0
				_write_vmcs(service, Vmcs::TSC_OFF_LO,  _tsc_offset & 0xffffffffu);
				_write_vmcs(service, Vmcs::TSC_OFF_HI, (_tsc_offset >> 32) & 0xffffffffu);
#endif
			}

			if (state.star.valid() || state.lstar.valid() ||
			    state.fmask.valid() || state.kernel_gs_base.valid())
			{
				if (_show_error_unsupported_star) {
					_show_error_unsupported_star = false;
					Genode::error("star, lstar, fmask, gs_base not supported by seL4");
				}
			}

			if (state.tpr.valid() || state.tpr_threshold.valid())
			{
				if (_show_error_unsupported_tpr)
				{
					_show_error_unsupported_tpr = false;
					Genode::error("tpr* not supported by seL4");
				}
			}

			if (state.dr7.valid())
				_write_vmcs(service, Vmcs::DR7, state.dr7.value());

			if (state.cr0.valid()) {
				_write_vmcs(service, Vmcs::CR0, cr0_set | (~cr0_mask & state.cr0.value()));
				_write_vmcs(service, Vmcs::CR0_SHADOW, state.cr0.value());
			}

			/* not supported on seL4 - state.cr2.valid() */

			if (state.cr3.valid())
				_write_vmcs(service, Vmcs::CR3, state.cr3.value());

			if (state.cr4.valid()) {
				_write_vmcs(service, Vmcs::CR4, cr4_set | (~cr4_mask & state.cr4.value()));
				_write_vmcs(service, Vmcs::CR4_SHADOW, state.cr4.value());
			}

			if (state.inj_info.valid()) {
				addr_t ctrl_0 = state.ctrl_primary.valid() ?
				                state.ctrl_primary.value() :
				                _read_vmcs(service, Vmcs::CTRL_0);

				if (state.inj_info.value() & 0x2000)
					Genode::warning("inj_info for NMI not supported");

				if (state.inj_info.value() & 0x1000)
					ctrl_0 |= Vmcs::IRQ_WINDOW;
				else
					ctrl_0 &= ~Vmcs::IRQ_WINDOW;

				state.ctrl_primary.value(ctrl_0);
			}

			if (state.inj_error.valid())
				_write_vmcs(service, Vmcs::INTR_ERROR, state.inj_error.value());

			if (state.flags.valid())
				_write_vmcs(service, Vmcs::RFLAGS, state.flags.value());

			if (state.sp.valid())
				_write_vmcs(service, Vmcs::RSP, state.sp.value());

			if (state.ip.valid())
				_write_vmcs(service, Vmcs::RIP, state.ip.value());

			if (state.ip_len.valid())
				_write_vmcs(service, Vmcs::ENTRY_INST_LEN, state.ip_len.value());

			if (state.efer.valid())
				_write_vmcs(service, Vmcs::EFER, state.efer.value());

			/* state.ctrl_primary.valid() update on vmenter - see above */

			if (state.ctrl_secondary.valid())
				_write_vmcs(service, Vmcs::CTRL_1, state.ctrl_secondary.value());

			if (state.intr_state.valid())
				_write_vmcs(service, Vmcs::STATE_INTR, state.intr_state.value());

			if (state.actv_state.valid())
				_write_vmcs(service, Vmcs::STATE_ACTV, state.actv_state.value());

			if (state.cs.valid()) {
				_write_vmcs(service, Vmcs::CS_SEL,   state.cs.value().sel);
				_write_vmcs(service, Vmcs::CS_LIMIT, state.cs.value().limit);
				_write_vmcs(service, Vmcs::CS_AR, _convert_ar(state.cs.value().ar));
				_write_vmcs(service, Vmcs::CS_BASE,  state.cs.value().base);
			}

			if (state.ss.valid()) {
				_write_vmcs(service, Vmcs::SS_SEL,   state.ss.value().sel);
				_write_vmcs(service, Vmcs::SS_LIMIT, state.ss.value().limit);
				_write_vmcs(service, Vmcs::SS_AR, _convert_ar(state.ss.value().ar));
				_write_vmcs(service, Vmcs::SS_BASE,  state.ss.value().base);
			}

			if (state.es.valid()) {
				_write_vmcs(service, Vmcs::ES_SEL,   state.es.value().sel);
				_write_vmcs(service, Vmcs::ES_LIMIT, state.es.value().limit);
				_write_vmcs(service, Vmcs::ES_AR, _convert_ar(state.es.value().ar));
				_write_vmcs(service, Vmcs::ES_BASE,  state.es.value().base);
			}

			if (state.ds.valid()) {
				_write_vmcs(service, Vmcs::DS_SEL,   state.ds.value().sel);
				_write_vmcs(service, Vmcs::DS_LIMIT, state.ds.value().limit);
				_write_vmcs(service, Vmcs::DS_AR, _convert_ar(state.ds.value().ar));
				_write_vmcs(service, Vmcs::DS_BASE,  state.ds.value().base);
			}

			if (state.fs.valid()) {
				_write_vmcs(service, Vmcs::FS_SEL,   state.fs.value().sel);
				_write_vmcs(service, Vmcs::FS_LIMIT, state.fs.value().limit);
				_write_vmcs(service, Vmcs::FS_AR, _convert_ar(state.fs.value().ar));
				_write_vmcs(service, Vmcs::FS_BASE,  state.fs.value().base);
			}

			if (state.gs.valid()) {
				_write_vmcs(service, Vmcs::GS_SEL,   state.gs.value().sel);
				_write_vmcs(service, Vmcs::GS_LIMIT, state.gs.value().limit);
				_write_vmcs(service, Vmcs::GS_AR, _convert_ar(state.gs.value().ar));
				_write_vmcs(service, Vmcs::GS_BASE,  state.gs.value().base);
			}

			if (state.tr.valid()) {
				_write_vmcs(service, Vmcs::TR_SEL,   state.tr.value().sel);
				_write_vmcs(service, Vmcs::TR_LIMIT, state.tr.value().limit);
				_write_vmcs(service, Vmcs::TR_AR, _convert_ar(state.tr.value().ar));
				_write_vmcs(service, Vmcs::TR_BASE,  state.tr.value().base);
			}

			if (state.ldtr.valid()) {
				_write_vmcs(service, Vmcs::LDTR_SEL,   state.ldtr.value().sel);
				_write_vmcs(service, Vmcs::LDTR_LIMIT, state.ldtr.value().limit);
				_write_vmcs(service, Vmcs::LDTR_AR, _convert_ar(state.ldtr.value().ar));
				_write_vmcs(service, Vmcs::LDTR_BASE,  state.ldtr.value().base);
			}

			if (state.idtr.valid()) {
				_write_vmcs(service, Vmcs::IDTR_BASE,  state.idtr.value().base);
				_write_vmcs(service, Vmcs::IDTR_LIMIT, state.idtr.value().limit);
			}

			if (state.gdtr.valid()) {
				_write_vmcs(service, Vmcs::GDTR_BASE,  state.gdtr.value().base);
				_write_vmcs(service, Vmcs::GDTR_LIMIT, state.gdtr.value().limit);
			}

			if (state.pdpte_0.valid())
				_write_vmcs(service, Vmcs::PDPTE_0, state.pdpte_0.value());

			if (state.pdpte_1.valid())
				_write_vmcs(service, Vmcs::PDPTE_1, state.pdpte_1.value());

			if (state.pdpte_2.valid())
				_write_vmcs(service, Vmcs::PDPTE_2, state.pdpte_2.value());

			if (state.pdpte_3.valid())
				_write_vmcs(service, Vmcs::PDPTE_3, state.pdpte_3.value());

			if (state.sysenter_cs.valid())
				_write_vmcs(service, Vmcs::SYSENTER_CS, state.sysenter_cs.value());

			if (state.sysenter_sp.valid())
				_write_vmcs(service, Vmcs::SYSENTER_SP, state.sysenter_sp.value());

			if (state.sysenter_ip.valid())
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

		void _read_sel4_state(seL4_X86_VCPU const service, Vm_state &state)
		{
			state.ip.value(seL4_GetMR(SEL4_VMENTER_CALL_EIP_MR));
			state.ctrl_primary.value(seL4_GetMR(SEL4_VMENTER_CALL_CONTROL_PPC_MR));

			state.ip_len.value(seL4_GetMR(SEL4_VMENTER_FAULT_INSTRUCTION_LEN_MR));
			state.qual_primary.value(seL4_GetMR(SEL4_VMENTER_FAULT_QUALIFICATION_MR));
			state.qual_secondary.value(seL4_GetMR(SEL4_VMENTER_FAULT_GUEST_PHYSICAL_MR));

			state.flags.value(seL4_GetMR(SEL4_VMENTER_FAULT_RFLAGS_MR));
			state.intr_state.value(seL4_GetMR(SEL4_VMENTER_FAULT_GUEST_INT_MR));
			state.cr3.value(seL4_GetMR(SEL4_VMENTER_FAULT_CR3_MR));

			state.ax.value(seL4_GetMR(SEL4_VMENTER_FAULT_EAX));
			state.bx.value(seL4_GetMR(SEL4_VMENTER_FAULT_EBX));
			state.cx.value(seL4_GetMR(SEL4_VMENTER_FAULT_ECX));
			state.dx.value(seL4_GetMR(SEL4_VMENTER_FAULT_EDX));
			state.si.value(seL4_GetMR(SEL4_VMENTER_FAULT_ESI));
			state.di.value(seL4_GetMR(SEL4_VMENTER_FAULT_EDI));
			state.bp.value(seL4_GetMR(SEL4_VMENTER_FAULT_EBP));

			_recent_gpr.eax = state.ax.value();
			_recent_gpr.ebx = state.bx.value();
			_recent_gpr.ecx = state.cx.value();
			_recent_gpr.edx = state.dx.value();
			_recent_gpr.esi = state.si.value();
			_recent_gpr.edi = state.di.value();
			_recent_gpr.ebp = state.bp.value();

			state.sp.value(_read_vmcs(service, Vmcs::RSP));
			state.dr7.value(_read_vmcs(service, Vmcs::DR7));

			/* r8 - r15 not supported on seL4 */

			{
				addr_t const cr0 = _read_vmcs(service, Vmcs::CR0);
				addr_t const cr0_shadow = _read_vmcs(service, Vmcs::CR0_SHADOW);
				state.cr0.value((cr0 & ~cr0_mask) | (cr0_shadow & cr0_mask));

				if (state.cr0.value() != cr0_shadow)
					_write_vmcs(service, Vmcs::CR0_SHADOW, state.cr0.value());
			}

			/* cr2 not supported on seL4 */
			state.cr2.value(state.cr2.value());

			{
				addr_t const cr4 = _read_vmcs(service, Vmcs::CR4);
				addr_t const cr4_shadow = _read_vmcs(service, Vmcs::CR4_SHADOW);
				state.cr4.value((cr4 & ~cr4_mask) | (cr4_shadow & cr4_mask));

				if (state.cr4.value() != cr4_shadow)
					_write_vmcs(service, Vmcs::CR4_SHADOW, state.cr4.value());
			}

			typedef Genode::Vm_state::Segment Segment;
			typedef Genode::Vm_state::Range Range;

			state.cs.value(Segment{_read_vmcs_16(service, Vmcs::CS_SEL),
			                       _convert_ar_16(_read_vmcs(service, Vmcs::CS_AR)),
			                       _read_vmcs_32(service, Vmcs::CS_LIMIT),
			                       _read_vmcs(service, Vmcs::CS_BASE)});

			state.ss.value(Segment{_read_vmcs_16(service, Vmcs::SS_SEL),
			                       _convert_ar_16(_read_vmcs(service, Vmcs::SS_AR)),
			                       _read_vmcs_32(service, Vmcs::SS_LIMIT),
			                       _read_vmcs(service, Vmcs::SS_BASE)});

			state.es.value(Segment{_read_vmcs_16(service, Vmcs::ES_SEL),
			                       _convert_ar_16(_read_vmcs(service, Vmcs::ES_AR)),
			                       _read_vmcs_32(service, Vmcs::ES_LIMIT),
			                       _read_vmcs(service, Vmcs::ES_BASE)});

			state.ds.value(Segment{_read_vmcs_16(service, Vmcs::DS_SEL),
			                       _convert_ar_16(_read_vmcs(service, Vmcs::DS_AR)),
			                       _read_vmcs_32(service, Vmcs::DS_LIMIT),
			                       _read_vmcs(service, Vmcs::DS_BASE)});

			state.fs.value(Segment{_read_vmcs_16(service, Vmcs::FS_SEL),
			                       _convert_ar_16(_read_vmcs(service, Vmcs::FS_AR)),
			                       _read_vmcs_32(service, Vmcs::FS_LIMIT),
			                       _read_vmcs(service, Vmcs::FS_BASE)});

			state.gs.value(Segment{_read_vmcs_16(service, Vmcs::GS_SEL),
			                       _convert_ar_16(_read_vmcs(service, Vmcs::GS_AR)),
			                       _read_vmcs_32(service, Vmcs::GS_LIMIT),
			                       _read_vmcs(service, Vmcs::GS_BASE)});

			state.tr.value(Segment{_read_vmcs_16(service, Vmcs::TR_SEL),
			                       _convert_ar_16(_read_vmcs(service, Vmcs::TR_AR)),
			                       _read_vmcs_32(service, Vmcs::TR_LIMIT),
			                       _read_vmcs(service, Vmcs::TR_BASE)});

			state.ldtr.value(Segment{_read_vmcs_16(service, Vmcs::LDTR_SEL),
			                         _convert_ar_16(_read_vmcs(service, Vmcs::LDTR_AR)),
			                         _read_vmcs_32(service, Vmcs::LDTR_LIMIT),
			                         _read_vmcs(service, Vmcs::LDTR_BASE)});

			state.idtr.value(Range{_read_vmcs(service, Vmcs::IDTR_BASE),
			                       _read_vmcs_32(service, Vmcs::IDTR_LIMIT)});

			state.gdtr.value(Range{_read_vmcs(service, Vmcs::GDTR_BASE),
			                       _read_vmcs_32(service, Vmcs::GDTR_LIMIT)});

			state.sysenter_cs.value(_read_vmcs(service, Vmcs::SYSENTER_CS));
			state.sysenter_sp.value(_read_vmcs(service, Vmcs::SYSENTER_SP));
			state.sysenter_ip.value(_read_vmcs(service, Vmcs::SYSENTER_IP));

			/* no support by seL4 to read this value */
			state.ctrl_secondary.value(state.ctrl_secondary.value());
			//state.ctrl_secondary.value(_read_vmcs(service, Vmcs::CTRL_1));

			if (state.exit_reason == VMEXIT_INVALID ||
			    state.exit_reason == VMEXIT_RECALL)
			{
				state.inj_info.value(_read_vmcs(service, Vmcs::INTR_INFO));
				state.inj_error.value(_read_vmcs(service, Vmcs::INTR_ERROR));
			} else {
				state.inj_info.value(_read_vmcs(service, Vmcs::IDT_INFO));
				state.inj_error.value(_read_vmcs(service, Vmcs::IDT_ERROR));
			}

			state.intr_state.value(_read_vmcs(service, Vmcs::STATE_INTR));
			state.actv_state.value(_read_vmcs(service, Vmcs::STATE_ACTV));

			state.pdpte_0.value(_read_vmcs(service, Vmcs::PDPTE_0));
			state.pdpte_1.value(_read_vmcs(service, Vmcs::PDPTE_1));
			state.pdpte_2.value(_read_vmcs(service, Vmcs::PDPTE_2));
			state.pdpte_3.value(_read_vmcs(service, Vmcs::PDPTE_3));

			/* tsc and tsc_offset not supported by seL4 */
			state.tsc.value(Trace::timestamp());
			state.tsc_offset.value(_tsc_offset);

			state.efer.value(_read_vmcs(service, Vmcs::EFER));

			/* XXX star, lstar, fmask, kernel_gs_base not supported by seL4 */

			/* XXX tpr and tpr_threshold not supported by seL4 */
		}

	public:

		Vcpu(Genode::Env &env, Genode::Signal_context_capability &cap,
		     Semaphore &handler_ready, Allocator &alloc,
		     Affinity::Location &location)
		:
			Thread(env, "vcpu_thread", STACK_SIZE, location, Weight(), env.cpu()),
			_signal(cap),
			_handler_ready(handler_ready), _alloc(alloc)
		{ }

		Allocator &allocator() { return _alloc; }

		void start() override {
			Thread::start();
			_startup.block();
		}

		Genode::Vm_session_client::Vcpu_id id() const  { return _id; }
		void id(Genode::Vm_session_client::Vcpu_id id) { _id = id;   }

		void assign_ds_state(Region_map &rm, Dataspace_capability cap) {
			_state = rm.attach(cap); }

		void initial_resume()
		{
			_wake_up.up();
		}

		void resume()
		{
			Mutex::Guard guard(_remote_mutex);

			if (_remote == RUN || _remote == PAUSE)
				return;

			_remote = RUN;
			_wake_up.up();
		}

		void pause()
		{
			Mutex::Guard guard(_remote_mutex);

			if (_remote == PAUSE)
				return;

			_remote = PAUSE;

			seL4_Signal(_recall);

			_wake_up.up();
		}
};

Genode::Vm_session_client::Vcpu_id
Genode::Vm_session_client::create_vcpu(Allocator &alloc, Env &env,
                                       Vm_handler_base &handler)
{
	Thread * ep = reinterpret_cast<Thread *>(&handler._rpc_ep);
	Affinity::Location location = ep->affinity();

	/* create thread that switches modes between thread/cpu */
	Vcpu * vcpu = new (alloc) Genode::Registered<Vcpu> (vcpus, env,
	                                                    handler._cap,
	                                                    handler._done,
	                                                    alloc,
	                                                    location);

	try {
		/* now it gets actually valid - vcpu->cap() becomes valid */
		vcpu->start();

		/* instruct core to let it become a vCPU */
		vcpu->id(call<Rpc_create_vcpu>(vcpu->cap()));
		call<Rpc_exception_handler>(handler._cap, vcpu->id());

		vcpu->assign_ds_state(env.rm(), call<Rpc_cpu_state>(vcpu->id()));
	} catch (...) {
		destroy(alloc, vcpu);
		throw;
	}

	vcpu->initial_resume();

	return vcpu->id();
}

void Genode::Vm_session_client::run(Genode::Vm_session_client::Vcpu_id id)
{
	vcpus.for_each([&] (Vcpu &vcpu) {
		if (vcpu.id().id == id.id)
			vcpu.resume();
	});
}

void Vm_session_client::pause(Vm_session_client::Vcpu_id vcpu_id)
{
	vcpus.for_each([&] (Vcpu &vcpu) {
		if (vcpu.id().id != vcpu_id.id)
			return;

		vcpu.pause();
	});
}

Genode::Dataspace_capability Genode::Vm_session_client::cpu_state(Vcpu_id vcpu_id)
{
	Dataspace_capability cap;

	cap = call<Rpc_cpu_state>(vcpu_id);

	return cap;
}

Vm_session::~Vm_session()
{
	vcpus.for_each([&] (Vcpu &vc) {
		Allocator &alloc = vc.allocator();
		destroy(alloc, &vc);
	});
}
