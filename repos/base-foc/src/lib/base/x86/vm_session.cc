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
#include <base/attached_rom_dataspace.h>
#include <base/env.h>
#include <base/registry.h>
#include <vm_session/client.h>

#include <cpu/vm_state.h>

#include <trace/timestamp.h>

namespace Fiasco {
#include <l4/sys/consts.h>
#include <l4/sys/irq.h>
#include <l4/sys/thread.h>
#include <l4/sys/types.h>
#include <l4/sys/vcpu.h>
#include <l4/sys/__vm-svm.h>
#include <l4/sys/__vm-vmx.h>
}

#include <foc/native_thread.h>
#include <foc/native_capability.h>

enum Virt { VMX, SVM, UNKNOWN };

using namespace Genode;

static uint32_t svm_features()
{
	uint32_t cpuid = 0x8000000a, edx = 0, ebx = 0, ecx = 0;
	asm volatile ("cpuid" : "+a" (cpuid), "=d" (edx), "=b"(ebx), "=c"(ecx));
	return edx;
}

static bool svm_np() { return svm_features() & (1U << 0); }

struct Vcpu;

static Genode::Registry<Genode::Registered<Vcpu> > vcpus;

struct Vcpu : Genode::Thread
{
	private:

		enum Vmcs {
			IRQ_WINDOW = 1U << 2,

			EXI_REASON = 0x4402,
			DR7        = 0x681a,

			CR0        = 0x6800,
			CR0_MASK   = 0x6000,
			CR4_MASK   = 0x6002,
			CR0_SHADOW = 0x6004,
			CR4_SHADOW = 0x6006,

			CR3        = 0x6802,
			CR4        = 0x6804,
			SP         = 0x681c,
			IP         = 0x681e,
			INST_LEN   = 0x440c,
			FLAGS      = 0x6820,
			EFER       = 0x2806,
			CTRL_0     = 0x4002,
			CTRL_1     = 0x401e,

			CS_SEL     = 0x0802,
			CS_LIMIT   = 0x4802,
			CS_AR      = 0x4816,
			CS_BASE    = 0x6808,

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

			GUEST_PHYS = 0x2400,
			EXIT_QUAL  = 0x6400,

			SYSENTER_CS = 0x482a,
			SYSENTER_SP = 0x6824,
			SYSENTER_IP = 0x6826,

			STATE_INTR  = 0x4824,
			STATE_ACTV  = 0x4826,

			INTR_INFO  = 0x4016,
			INTR_ERROR = 0x4018,
			ENTRY_INST_LEN = 0x401a,

			IDT_INFO  = 0x4408,
			IDT_ERROR = 0x440a,

			EXIT_CTRL = 0x400c,
			ENTRY_CTRL = 0x4012,

			TSC_OFF_LO = 0x2010,
			TSC_OFF_HI = 0x2011,

			MSR_FMASK  = 0x2842,
			MSR_LSTAR  = 0x2844,
			MSR_STAR   = 0x284a,
			KERNEL_GS_BASE = 0x284c,

			CR4_VMX        = 1 << 13,
			INTEL_EXIT_INVALID = 0x21,

		};

		enum Vmcb
		{
			CTRL0_VINTR       = 1u << 4,
			CTRL0_IO          = 1u << 27,
			CTRL0_MSR         = 1u << 28,

			AMD_SVM_ENABLE    = 1 << 12,

			AMD_EXIT_INVALID  = 0xfd,
		};

		enum {
			CR0_PE = 0, /* 1U << 0 - not needed in case of UG */
			CR0_CP = 1U << 1,
			CR0_TS = 1u << 3,
			CR0_NE = 1U << 5,
			CR0_NM = 1U << 29,
			CR0_CD = 1U << 30,
			CR0_PG = 0 /* 1U << 31 - not needed in case of UG */
		};

		addr_t const _cr0_mask       { CR0_NM | CR0_CD };
		addr_t const vmcb_ctrl0      { CTRL0_IO | CTRL0_MSR };
		addr_t const vmcb_ctrl1      { 0 };

		addr_t       vmcb_cr0_shadow { 0 };
		addr_t       vmcb_cr4_shadow { 0 };
		addr_t const vmcb_cr0_mask   { _cr0_mask };
		addr_t const vmcb_cr0_set    { 0 };
		addr_t const vmcb_cr4_mask   { 0 };
		addr_t const vmcb_cr4_set    { 0 };

		enum { EXIT_ON_HLT = 1U << 7 };
		addr_t const _vmcs_ctrl0     { EXIT_ON_HLT };

		addr_t const vmcs_cr0_mask   { _cr0_mask | CR0_CP | CR0_NE | CR0_PE | CR0_PG };
		addr_t const vmcs_cr0_set    { 0 };

		addr_t const vmcs_cr4_mask   { CR4_VMX };
		addr_t const vmcs_cr4_set    { CR4_VMX };

		Signal_context_capability   _signal;
		Semaphore                   _wake_up { 0 };
		Semaphore                  &_handler_ready;
		Allocator                  &_alloc;
		Vm_session_client::Vcpu_id  _id { Vm_session_client::Vcpu_id::INVALID };
		addr_t                      _state { 0 };
		addr_t                      _task { 0 };
		enum Virt const             _vm_type;
		uint64_t                    _tsc_offset { 0 };
		bool                        _show_error_unsupported_pdpte { true };
		bool                        _show_error_unsupported_tpr   { true };
		bool                        _show_error_unsupported_fpu   { true };

		enum
		{
			VMEXIT_STARTUP = 0xfe,
			VMEXIT_PAUSED  = 0xff,
			STACK_SIZE     = 0x3000,
		};


		enum State {
			NONE = 0,
			PAUSE = 1,
			RUN = 2,
			TERMINATE = 3,
		};

		State _state_request { NONE };
		State _state_current { NONE };
		Mutex _remote_mutex  { };

		void entry() override
		{
			_wake_up.down();

			{
				Mutex::Guard guard(_remote_mutex);

				/* leave scope for Thread::join() - vCPU setup failed */
				if (_state_request == TERMINATE)
					return;

				_state_request = NONE;
			}

			/* reserved ranged for state of vCPUs - see platform.cc */
			Genode::addr_t const vcpu_addr = 0x1000 + 0x1000 * _id.id;
			Fiasco::l4_vcpu_state_t * const vcpu = reinterpret_cast<Fiasco::l4_vcpu_state_t*>(vcpu_addr);

			if (!l4_vcpu_check_version(vcpu))
				Genode::error("vCPU version mismatch kernel vs user-land - ",
				              vcpu->version, "!=",
				              (int)Fiasco::L4_VCPU_STATE_VERSION);

			using namespace Fiasco;
			l4_vm_svm_vmcb_t *vmcb = reinterpret_cast<l4_vm_svm_vmcb_t *>(vcpu_addr + L4_VCPU_OFFSET_EXT_STATE);
			void * vmcs = reinterpret_cast<void *>(vcpu_addr + L4_VCPU_OFFSET_EXT_STATE);

			/* set vm page table */
			vcpu->user_task = _task;

			Vm_state &state = *reinterpret_cast<Vm_state *>(_state);
			state = Vm_state {};

			/* initial startup VM exit to get valid VM state */
			if (_vm_type == Virt::VMX) {
				_read_intel_state(state, vmcs, vcpu);
			}
			if (_vm_type == Virt::SVM) {
				_read_amd_state(state, vmcb, vcpu);
			}

			state.exit_reason = VMEXIT_STARTUP;
			Genode::Signal_transmitter(_signal).submit();

			_handler_ready.down();
			_wake_up.down();

			/*
			 * Fiasoc.OC peculiarities
			 */
			if (_vm_type == Virt::SVM) {
				state.efer.value(state.efer.value() | AMD_SVM_ENABLE);
			}
			if (_vm_type == Virt::SVM) {
				vmcb->control_area.intercept_instruction0 = vmcb_ctrl0;
				vmcb->control_area.intercept_instruction1 = vmcb_ctrl1;

				/* special handling on missing NPT support */
				vmcb->control_area.np_enable = svm_np();
				if (!vmcb->control_area.np_enable) {
					vmcb->control_area.intercept_exceptions |= 1 << 14;
					vmcb->control_area.intercept_rd_crX = 0x0001; /* cr0 */
					vmcb->control_area.intercept_wr_crX = 0x0001; /* cr0 */
				} else
					vmcb->state_save_area.g_pat = 0x7040600070406ull;
			}
			if (_vm_type == Virt::VMX) {
				Fiasco::l4_vm_vmx_write(vmcs, Vmcs::CR0_MASK, vmcs_cr0_mask);
				Fiasco::l4_vm_vmx_write(vmcs, Vmcs::CR4_MASK, vmcs_cr4_mask);
				Fiasco::l4_vm_vmx_write(vmcs, Vmcs::CR4_SHADOW, 0);
				state.cr4.value(vmcs_cr4_set);

				enum {
					EXIT_SAVE_EFER = 1U << 20,
					ENTRY_LOAD_EFER = 1U << 15,
				};
				Fiasco::l4_vm_vmx_write(vmcs, Vmcs::EXIT_CTRL, EXIT_SAVE_EFER);
				Fiasco::l4_vm_vmx_write(vmcs, Vmcs::ENTRY_CTRL, ENTRY_LOAD_EFER);
			}

			if (_vm_type == Virt::SVM)
				_write_amd_state(state, vmcb, vcpu);
			if (_vm_type == Virt::VMX)
				_write_intel_state(state, vmcs, vcpu);

			vcpu->saved_state = L4_VCPU_F_USER_MODE | L4_VCPU_F_FPU_ENABLED;

			while (true) {
				/* read in requested state from remote threads */
				{
					Mutex::Guard guard(_remote_mutex);
					_state_current = _state_request;
					_state_request = NONE;
				}

				if (_state_current == NONE) {
					_wake_up.down();
					continue;
				}

				if (_state_current != RUN && _state_current != PAUSE) {
					Genode::error("unknown vcpu state ", (int)_state_current);
					while (true) { _remote_mutex.acquire(); }
				}

				/* transfer vCPU state to Fiasco.OC */
				if (_vm_type == Virt::SVM)
					_write_amd_state(state, vmcb, vcpu);
				if (_vm_type == Virt::VMX)
					_write_intel_state(state, vmcs, vcpu);

				/* tell Fiasco.OC to run the vCPU */
				l4_msgtag_t tag = l4_thread_vcpu_resume_start();
				tag = l4_thread_vcpu_resume_commit(L4_INVALID_CAP, tag);

				/* got VM exit or interrupted by asynchronous signal */
				uint64_t reason = 0;

				state = Vm_state {};

				if (_vm_type == Virt::SVM) {
					reason = vmcb->control_area.exitcode;
					if (reason == 0x400) /* no NPT support */
						reason = 0xfc; 

					{
						Mutex::Guard guard(_remote_mutex);
						_state_request = NONE;
						_state_current = PAUSE;

						/* remotely PAUSE was called */
						if (l4_error(tag) && reason == 0x60) {
							reason = VMEXIT_PAUSED;

							/* consume notification */
							while (vcpu->sticky_flags) {
								Fiasco::l4_cap_idx_t tid = native_thread().kcap;
								Fiasco::l4_cap_idx_t irq = tid + Fiasco::TASK_VCPU_IRQ_CAP;
								l4_irq_receive(irq, L4_IPC_RECV_TIMEOUT_0);
							}
						}
					}

					state.exit_reason = reason & 0xff;
					_read_amd_state(state, vmcb, vcpu);
				}

				if (_vm_type == Virt::VMX) {
					reason = Fiasco::l4_vm_vmx_read_32(vmcs, Vmcs::EXI_REASON);

					{
						Mutex::Guard guard(_remote_mutex);
						_state_request = NONE;
						_state_current = PAUSE;

						/* remotely PAUSE was called */
						if (l4_error(tag) && reason == 0x1) {
							reason = VMEXIT_PAUSED;

							/* consume notification */
							while (vcpu->sticky_flags) {
								Fiasco::l4_cap_idx_t tid = native_thread().kcap;
								Fiasco::l4_cap_idx_t irq = tid + Fiasco::TASK_VCPU_IRQ_CAP;
								l4_irq_receive(irq, L4_IPC_RECV_TIMEOUT_0);
							}
						}
					}

					state.exit_reason = reason & 0xff;
					_read_intel_state(state, vmcs, vcpu);
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

		void _read_intel_state(Vm_state &state, void *vmcs,
		                       Fiasco::l4_vcpu_state_t *vcpu)
		{
			state.ax.value(vcpu->r.ax);
			state.cx.value(vcpu->r.cx);
			state.dx.value(vcpu->r.dx);
			state.bx.value(vcpu->r.bx);

			state.bp.value(vcpu->r.bp);
			state.di.value(vcpu->r.di);
			state.si.value(vcpu->r.si);

			state.flags.value(Fiasco::l4_vm_vmx_read(vmcs, Vmcs::FLAGS));

			state.sp.value(Fiasco::l4_vm_vmx_read(vmcs, Vmcs::SP));

			state.ip.value(Fiasco::l4_vm_vmx_read(vmcs, Vmcs::IP));
			state.ip_len.value(Fiasco::l4_vm_vmx_read(vmcs, Vmcs::INST_LEN));

			state.dr7.value(Fiasco::l4_vm_vmx_read(vmcs, Vmcs::DR7));

#ifdef __x86_64__
			state.r8.value(vcpu->r.r8);
			state.r9.value(vcpu->r.r9);
			state.r10.value(vcpu->r.r10);
			state.r11.value(vcpu->r.r11);
			state.r12.value(vcpu->r.r12);
			state.r13.value(vcpu->r.r13);
			state.r14.value(vcpu->r.r14);
			state.r15.value(vcpu->r.r15);
#endif

			{
				addr_t const cr0 = Fiasco::l4_vm_vmx_read(vmcs, Vmcs::CR0);
				addr_t const cr0_shadow = Fiasco::l4_vm_vmx_read(vmcs, Vmcs::CR0_SHADOW);
				state.cr0.value((cr0 & ~vmcs_cr0_mask) | (cr0_shadow & vmcs_cr0_mask));
				if (state.cr0.value() != cr0_shadow)
					Fiasco::l4_vm_vmx_write(vmcs, Vmcs::CR0_SHADOW, state.cr0.value());
			}

			unsigned const cr2 = Fiasco::l4_vm_vmx_get_cr2_index(vmcs);
			state.cr2.value(Fiasco::l4_vm_vmx_read(vmcs, cr2));
			state.cr3.value(Fiasco::l4_vm_vmx_read(vmcs, Vmcs::CR3));

			{
				addr_t const cr4 = Fiasco::l4_vm_vmx_read(vmcs, Vmcs::CR4);
				addr_t const cr4_shadow = Fiasco::l4_vm_vmx_read(vmcs, Vmcs::CR4_SHADOW);
				state.cr4.value((cr4 & ~vmcs_cr4_mask) | (cr4_shadow & vmcs_cr4_mask));
				if (state.cr4.value() != cr4_shadow)
					Fiasco::l4_vm_vmx_write(vmcs, Vmcs::CR4_SHADOW,
					                        state.cr4.value());
			}

			using Fiasco::l4_vm_vmx_read;
			using Fiasco::l4_vm_vmx_read_16;
			using Fiasco::l4_vm_vmx_read_32;
			using Fiasco::l4_vm_vmx_read_nat;
			typedef Genode::Vm_state::Segment Segment;
			typedef Genode::Vm_state::Range Range;

			{
				Segment cs { l4_vm_vmx_read_16(vmcs, Vmcs::CS_SEL),
				             _convert_ar_16(l4_vm_vmx_read(vmcs, Vmcs::CS_AR)),
				             l4_vm_vmx_read_32(vmcs, Vmcs::CS_LIMIT),
				             l4_vm_vmx_read_nat(vmcs, Vmcs::CS_BASE) };

				state.cs.value(cs);
			}

			{
				Segment ss { l4_vm_vmx_read_16(vmcs, Vmcs::SS_SEL),
				            _convert_ar_16(l4_vm_vmx_read(vmcs, Vmcs::SS_AR)),
				            l4_vm_vmx_read_32(vmcs, Vmcs::SS_LIMIT),
				            l4_vm_vmx_read_nat(vmcs, Vmcs::SS_BASE) };

				state.ss.value(ss);
			}

			{
				Segment es { l4_vm_vmx_read_16(vmcs, Vmcs::ES_SEL),
				             _convert_ar_16(l4_vm_vmx_read(vmcs, Vmcs::ES_AR)),
				             l4_vm_vmx_read_32(vmcs, Vmcs::ES_LIMIT),
				             l4_vm_vmx_read_nat(vmcs, Vmcs::ES_BASE) };

				state.es.value(es);
			}

			{
				Segment ds { l4_vm_vmx_read_16(vmcs, Vmcs::DS_SEL),
				             _convert_ar_16(l4_vm_vmx_read(vmcs, Vmcs::DS_AR)),
				             l4_vm_vmx_read_32(vmcs, Vmcs::DS_LIMIT),
				             l4_vm_vmx_read_nat(vmcs, Vmcs::DS_BASE) };

				state.ds.value(ds);
			}

			{
				Segment fs { l4_vm_vmx_read_16(vmcs, Vmcs::FS_SEL),
				             _convert_ar_16(l4_vm_vmx_read(vmcs, Vmcs::FS_AR)),
				             l4_vm_vmx_read_32(vmcs, Vmcs::FS_LIMIT),
				             l4_vm_vmx_read_nat(vmcs, Vmcs::FS_BASE) };

				state.fs.value(fs);
			}

			{
				Segment gs { l4_vm_vmx_read_16(vmcs, Vmcs::GS_SEL),
				             _convert_ar_16(l4_vm_vmx_read(vmcs, Vmcs::GS_AR)),
				             l4_vm_vmx_read_32(vmcs, Vmcs::GS_LIMIT),
				             l4_vm_vmx_read_nat(vmcs, Vmcs::GS_BASE) };

				state.gs.value(gs);
			}

			{
				Segment tr { l4_vm_vmx_read_16(vmcs, Vmcs::TR_SEL),
				             _convert_ar_16(l4_vm_vmx_read(vmcs, Vmcs::TR_AR)),
				             l4_vm_vmx_read_32(vmcs, Vmcs::TR_LIMIT),
				             l4_vm_vmx_read_nat(vmcs, Vmcs::TR_BASE) };

				state.tr.value(tr);
			}

			{
				Segment ldtr { l4_vm_vmx_read_16(vmcs, Vmcs::LDTR_SEL),
				               _convert_ar_16(l4_vm_vmx_read(vmcs, Vmcs::LDTR_AR)),
				               l4_vm_vmx_read_32(vmcs, Vmcs::LDTR_LIMIT),
				               l4_vm_vmx_read_nat(vmcs, Vmcs::LDTR_BASE) };

				state.ldtr.value(ldtr);
			}

			state.gdtr.value(Range{l4_vm_vmx_read_nat(vmcs, Vmcs::GDTR_BASE),
			                       l4_vm_vmx_read_32(vmcs, Vmcs::GDTR_LIMIT)});

			state.idtr.value(Range{l4_vm_vmx_read_nat(vmcs, Vmcs::IDTR_BASE),
			                       l4_vm_vmx_read_32(vmcs, Vmcs::IDTR_LIMIT)});

			state.sysenter_cs.value(l4_vm_vmx_read(vmcs, Vmcs::SYSENTER_CS));
			state.sysenter_sp.value(l4_vm_vmx_read(vmcs, Vmcs::SYSENTER_SP));
			state.sysenter_ip.value(l4_vm_vmx_read(vmcs, Vmcs::SYSENTER_IP));

			state.qual_primary.value(l4_vm_vmx_read(vmcs, Vmcs::EXIT_QUAL));
			state.qual_secondary.value(l4_vm_vmx_read(vmcs, Vmcs::GUEST_PHYS));

			state.ctrl_primary.value(l4_vm_vmx_read(vmcs, Vmcs::CTRL_0));
			state.ctrl_secondary.value(l4_vm_vmx_read(vmcs, Vmcs::CTRL_1));

			if (state.exit_reason == INTEL_EXIT_INVALID ||
			    state.exit_reason == VMEXIT_PAUSED)
			{
				state.inj_info.value(l4_vm_vmx_read(vmcs, Vmcs::INTR_INFO));
				state.inj_error.value(l4_vm_vmx_read(vmcs, Vmcs::INTR_ERROR));
			} else {
				state.inj_info.value(l4_vm_vmx_read(vmcs, Vmcs::IDT_INFO));
				state.inj_error.value(l4_vm_vmx_read(vmcs, Vmcs::IDT_ERROR));
			}

			state.intr_state.value(l4_vm_vmx_read(vmcs, Vmcs::STATE_INTR));
			state.actv_state.value(l4_vm_vmx_read(vmcs, Vmcs::STATE_ACTV));

			state.tsc.value(Trace::timestamp());
			state.tsc_offset.value(_tsc_offset);

			state.efer.value(l4_vm_vmx_read(vmcs, Vmcs::EFER));

			state.star.value(l4_vm_vmx_read(vmcs, Vmcs::MSR_STAR));
			state.lstar.value(l4_vm_vmx_read(vmcs, Vmcs::MSR_LSTAR));
			state.fmask.value(l4_vm_vmx_read(vmcs, Vmcs::MSR_FMASK));
			state.kernel_gs_base.value(l4_vm_vmx_read(vmcs, Vmcs::KERNEL_GS_BASE));

			/* XXX missing */
#if 0
			if (state.pdpte_0_updated() || state.pdpte_1_updated() ||
			if (state.tpr_updated() || state.tpr_threshold_updated()) {
#endif
		}

		void _read_amd_state(Vm_state &state, Fiasco::l4_vm_svm_vmcb_t *vmcb,
		                     Fiasco::l4_vcpu_state_t * const vcpu)
		{
			state.ax.value(vmcb->state_save_area.rax);
			state.cx.value(vcpu->r.cx);
			state.dx.value(vcpu->r.dx);
			state.bx.value(vcpu->r.bx);

			state.di.value(vcpu->r.di);
			state.si.value(vcpu->r.si);
			state.bp.value(vcpu->r.bp);

			state.flags.value(vmcb->state_save_area.rflags);

			state.sp.value(vmcb->state_save_area.rsp);

			state.ip.value(vmcb->state_save_area.rip);
			state.ip_len.value(0); /* unsupported on AMD */

			state.dr7.value(vmcb->state_save_area.dr7);

#ifdef __x86_64__
			state.r8.value(vcpu->r.r8);
			state.r9.value(vcpu->r.r9);
			state.r10.value(vcpu->r.r10);
			state.r11.value(vcpu->r.r11);
			state.r12.value(vcpu->r.r12);
			state.r13.value(vcpu->r.r13);
			state.r14.value(vcpu->r.r14);
			state.r15.value(vcpu->r.r15);
#endif

			{
				addr_t const cr0 = vmcb->state_save_area.cr0;
				state.cr0.value((cr0 & ~vmcb_cr0_mask) | (vmcb_cr0_shadow & vmcb_cr0_mask));
				if (state.cr0.value() != vmcb_cr0_shadow)
					vmcb_cr0_shadow = state.cr0.value();
			}
			state.cr2.value(vmcb->state_save_area.cr2);
			state.cr3.value(vmcb->state_save_area.cr3);
			{
				addr_t const cr4 = vmcb->state_save_area.cr4;
				state.cr4.value((cr4 & ~vmcb_cr4_mask) | (vmcb_cr4_shadow & vmcb_cr4_mask));
				if (state.cr4.value() != vmcb_cr4_shadow)
					vmcb_cr4_shadow = state.cr4.value();
			}

			typedef Genode::Vm_state::Segment Segment;

			state.cs.value(Segment{vmcb->state_save_area.cs.selector,
			                       vmcb->state_save_area.cs.attrib,
			                       vmcb->state_save_area.cs.limit,
			                       (addr_t)vmcb->state_save_area.cs.base});

			state.ss.value(Segment{vmcb->state_save_area.ss.selector,
			                       vmcb->state_save_area.ss.attrib,
			                       vmcb->state_save_area.ss.limit,
			                       (addr_t)vmcb->state_save_area.ss.base});

			state.es.value(Segment{vmcb->state_save_area.es.selector,
			                       vmcb->state_save_area.es.attrib,
			                       vmcb->state_save_area.es.limit,
			                       (addr_t)vmcb->state_save_area.es.base});

			state.ds.value(Segment{vmcb->state_save_area.ds.selector,
			                       vmcb->state_save_area.ds.attrib,
			                       vmcb->state_save_area.ds.limit,
			                       (addr_t)vmcb->state_save_area.ds.base});

			state.fs.value(Segment{vmcb->state_save_area.fs.selector,
			                       vmcb->state_save_area.fs.attrib,
			                       vmcb->state_save_area.fs.limit,
			                       (addr_t)vmcb->state_save_area.fs.base});

			state.gs.value(Segment{vmcb->state_save_area.gs.selector,
			                       vmcb->state_save_area.gs.attrib,
			                       vmcb->state_save_area.gs.limit,
			                       (addr_t)vmcb->state_save_area.gs.base});

			state.tr.value(Segment{vmcb->state_save_area.tr.selector,
			                       vmcb->state_save_area.tr.attrib,
			                       vmcb->state_save_area.tr.limit,
			                       (addr_t)vmcb->state_save_area.tr.base});

			state.ldtr.value(Segment{vmcb->state_save_area.ldtr.selector,
			                         vmcb->state_save_area.ldtr.attrib,
			                         vmcb->state_save_area.ldtr.limit,
			                         (addr_t)vmcb->state_save_area.ldtr.base});

			typedef Genode::Vm_state::Range Range;

			state.gdtr.value(Range{(addr_t)vmcb->state_save_area.gdtr.base,
			                       vmcb->state_save_area.gdtr.limit});

			state.idtr.value(Range{(addr_t)vmcb->state_save_area.idtr.base,
			                       vmcb->state_save_area.idtr.limit});

			state.sysenter_cs.value(vmcb->state_save_area.sysenter_cs);
			state.sysenter_sp.value(vmcb->state_save_area.sysenter_esp);
			state.sysenter_ip.value(vmcb->state_save_area.sysenter_eip);

			state.qual_primary.value(vmcb->control_area.exitinfo1);
			state.qual_secondary.value(vmcb->control_area.exitinfo2);

			uint32_t inj_info = 0;
			uint32_t inj_error = 0;
			if (state.exit_reason == AMD_EXIT_INVALID ||
			    state.exit_reason == VMEXIT_PAUSED)
			{
				inj_info  = vmcb->control_area.eventinj;
				inj_error = vmcb->control_area.eventinj >> 32;
			} else {
				inj_info  = vmcb->control_area.exitintinfo;
				inj_error = vmcb->control_area.exitintinfo >> 32;
			}
			state.inj_info.value(inj_info);
			state.inj_error.value(inj_error);

			state.intr_state.value(vmcb->control_area.interrupt_shadow);
			state.actv_state.value(0);

			state.tsc.value(Trace::timestamp());
			state.tsc_offset.value(_tsc_offset);

			state.efer.value(vmcb->state_save_area.efer);

			if (state.pdpte_0.valid() || state.pdpte_1.valid() ||
			    state.pdpte_2.valid() || state.pdpte_3.valid()) {

				Genode::error("pdpte not implemented");
			}

			if (state.star.valid() || state.lstar.valid() ||
			    state.fmask.valid() || state.kernel_gs_base.valid()) {

				Genode::error("star, fstar, fmask, kernel_gs_base not implemented");
			}

			if (state.tpr.valid() || state.tpr_threshold.valid()) {
				Genode::error("tpr not implemented");
			}
		}

		void _write_intel_state(Vm_state &state, void *vmcs,
		                        Fiasco::l4_vcpu_state_t *vcpu)
		{
			using Fiasco::l4_vm_vmx_write;

			if (state.ax.valid() || state.cx.valid() || state.dx.valid() ||
			    state.bx.valid()) {
				vcpu->r.ax = state.ax.value();
				vcpu->r.cx = state.cx.value();
				vcpu->r.dx = state.dx.value();
				vcpu->r.bx = state.bx.value();
			}

			if (state.bp.valid() || state.di.valid() || state.si.valid()) {
				vcpu->r.bp = state.bp.value();
				vcpu->r.di = state.di.value();
				vcpu->r.si = state.si.value();
			}

			if (state.r8.valid()  || state.r9.valid() || state.r10.valid() ||
			    state.r11.valid() || state.r12.valid() || state.r13.valid() ||
			    state.r14.valid() || state.r15.valid()) {
#ifdef __x86_64__
				vcpu->r.r8 = state.r8.value();
				vcpu->r.r9 = state.r9.value();
				vcpu->r.r10 = state.r10.value();
				vcpu->r.r11 = state.r11.value();
				vcpu->r.r12 = state.r12.value();
				vcpu->r.r13 = state.r13.value();
				vcpu->r.r14 = state.r14.value();
				vcpu->r.r15 = state.r15.value();
#endif
			}

			if (state.tsc_offset.valid()) {
				_tsc_offset += state.tsc_offset.value();
				l4_vm_vmx_write(vmcs, Vmcs::TSC_OFF_LO,  _tsc_offset & 0xffffffffu);
				l4_vm_vmx_write(vmcs, Vmcs::TSC_OFF_HI, (_tsc_offset >> 32) & 0xffffffffu);
			}

			if (state.star.valid())
				l4_vm_vmx_write(vmcs, Vmcs::MSR_STAR, state.star.value());

			if (state.lstar.valid())
				l4_vm_vmx_write(vmcs, Vmcs::MSR_LSTAR, state.lstar.value());

			if (state.fmask.valid())
				l4_vm_vmx_write(vmcs, Vmcs::MSR_FMASK, state.fmask.value());

			if (state.kernel_gs_base.valid())
				l4_vm_vmx_write(vmcs, Vmcs::KERNEL_GS_BASE, state.kernel_gs_base.value());

			if (state.tpr.valid() || state.tpr_threshold.valid()) {
				if (_show_error_unsupported_tpr) {
					_show_error_unsupported_tpr = false;
					Genode::error("TPR & TPR_THRESHOLD not supported on Fiasco.OC");
				}
			}

			if (state.dr7.valid())
				l4_vm_vmx_write(vmcs, Vmcs::DR7, state.dr7.value());

			if (state.cr0.valid()) {
				l4_vm_vmx_write(vmcs, Vmcs::CR0, vmcs_cr0_set | (~vmcs_cr0_mask & state.cr0.value()));
				l4_vm_vmx_write(vmcs, Vmcs::CR0_SHADOW, state.cr0.value());

				#if 0 /* if CPU has xsave feature bit ... see Vm::load_guest_xcr0 */
				l4_vm_vmx_write(vmcs, Fiasco::L4_VM_VMX_VMCS_XCR0, state.cr0.value());
				#endif
			}

			if (state.cr2.valid()) {
				unsigned const cr2 = Fiasco::l4_vm_vmx_get_cr2_index(vmcs);
				l4_vm_vmx_write(vmcs, cr2, state.cr2.value());
			}

			if (state.cr3.valid())
				l4_vm_vmx_write(vmcs, Vmcs::CR3, state.cr3.value());

			if (state.cr4.valid()) {
				l4_vm_vmx_write(vmcs, Vmcs::CR4,
				                vmcs_cr4_set | (~vmcs_cr4_mask & state.cr4.value()));
				l4_vm_vmx_write(vmcs, Vmcs::CR4_SHADOW, state.cr4.value());
			}

			if (state.inj_info.valid() || state.inj_error.valid()) {
				addr_t ctrl_0 = state.ctrl_primary.valid() ?
				                state.ctrl_primary.value() :
				                Fiasco::l4_vm_vmx_read(vmcs, Vmcs::CTRL_0);

				if (state.inj_info.value() & 0x2000)
					Genode::warning("unimplemented ", state.inj_info.value() & 0x1000, " ", state.inj_info.value() & 0x2000, " ", Genode::Hex(ctrl_0), " ", Genode::Hex(state.ctrl_secondary.value()));

				if (state.inj_info.value() & 0x1000)
					ctrl_0 |= Vmcs::IRQ_WINDOW;
				else
					ctrl_0 &= ~Vmcs::IRQ_WINDOW;

				state.ctrl_primary.value(ctrl_0);

				l4_vm_vmx_write(vmcs, Vmcs::INTR_INFO,
				                      state.inj_info.value() & ~0x3000);
				l4_vm_vmx_write(vmcs, Vmcs::INTR_ERROR,
				                      state.inj_error.value());
			}

			if (state.flags.valid())
				l4_vm_vmx_write(vmcs, Vmcs::FLAGS, state.flags.value());

			if (state.sp.valid())
				l4_vm_vmx_write(vmcs, Vmcs::SP, state.sp.value());

			if (state.ip.valid())
				l4_vm_vmx_write(vmcs, Vmcs::IP, state.ip.value());

			if (state.ip_len.valid())
				l4_vm_vmx_write(vmcs, Vmcs::ENTRY_INST_LEN, state.ip_len.value());

			if (state.efer.valid())
				l4_vm_vmx_write(vmcs, Vmcs::EFER, state.efer.value());

			if (state.ctrl_primary.valid())
				l4_vm_vmx_write(vmcs, Vmcs::CTRL_0,
				                _vmcs_ctrl0 | state.ctrl_primary.value());

			if (state.ctrl_secondary.valid())
				l4_vm_vmx_write(vmcs, Vmcs::CTRL_1,
				                state.ctrl_secondary.value());

			if (state.intr_state.valid())
				l4_vm_vmx_write(vmcs, Vmcs::STATE_INTR,
				                state.intr_state.value());

			if (state.actv_state.valid())
				l4_vm_vmx_write(vmcs, Vmcs::STATE_ACTV,
				                state.actv_state.value());

			if (state.cs.valid()) {
				l4_vm_vmx_write(vmcs, Vmcs::CS_SEL, state.cs.value().sel);
				l4_vm_vmx_write(vmcs, Vmcs::CS_AR, _convert_ar(state.cs.value().ar));
				l4_vm_vmx_write(vmcs, Vmcs::CS_LIMIT, state.cs.value().limit);
				l4_vm_vmx_write(vmcs, Vmcs::CS_BASE, state.cs.value().base);
			}

			if (state.ss.valid()) {
				l4_vm_vmx_write(vmcs, Vmcs::SS_SEL, state.ss.value().sel);
				l4_vm_vmx_write(vmcs, Vmcs::SS_AR, _convert_ar(state.ss.value().ar));
				l4_vm_vmx_write(vmcs, Vmcs::SS_LIMIT, state.ss.value().limit);
				l4_vm_vmx_write(vmcs, Vmcs::SS_BASE, state.ss.value().base);
			}

			if (state.es.valid()) {
				l4_vm_vmx_write(vmcs, Vmcs::ES_SEL, state.es.value().sel);
				l4_vm_vmx_write(vmcs, Vmcs::ES_AR, _convert_ar(state.es.value().ar));
				l4_vm_vmx_write(vmcs, Vmcs::ES_LIMIT, state.es.value().limit);
				l4_vm_vmx_write(vmcs, Vmcs::ES_BASE, state.es.value().base);
			}

			if (state.ds.valid()) {
				l4_vm_vmx_write(vmcs, Vmcs::DS_SEL, state.ds.value().sel);
				l4_vm_vmx_write(vmcs, Vmcs::DS_AR, _convert_ar(state.ds.value().ar));
				l4_vm_vmx_write(vmcs, Vmcs::DS_LIMIT, state.ds.value().limit);
				l4_vm_vmx_write(vmcs, Vmcs::DS_BASE, state.ds.value().base);
			}

			if (state.fs.valid()) {
				l4_vm_vmx_write(vmcs, Vmcs::FS_SEL, state.fs.value().sel);
				l4_vm_vmx_write(vmcs, Vmcs::FS_AR, _convert_ar(state.fs.value().ar));
				l4_vm_vmx_write(vmcs, Vmcs::FS_LIMIT, state.fs.value().limit);
				l4_vm_vmx_write(vmcs, Vmcs::FS_BASE, state.fs.value().base);
			}

			if (state.gs.valid()) {
				l4_vm_vmx_write(vmcs, Vmcs::GS_SEL, state.gs.value().sel);
				l4_vm_vmx_write(vmcs, Vmcs::GS_AR, _convert_ar(state.gs.value().ar));
				l4_vm_vmx_write(vmcs, Vmcs::GS_LIMIT, state.gs.value().limit);
				l4_vm_vmx_write(vmcs, Vmcs::GS_BASE, state.gs.value().base);
			}

			if (state.tr.valid()) {
				l4_vm_vmx_write(vmcs, Vmcs::TR_SEL, state.tr.value().sel);
				l4_vm_vmx_write(vmcs, Vmcs::TR_AR, _convert_ar(state.tr.value().ar));
				l4_vm_vmx_write(vmcs, Vmcs::TR_LIMIT, state.tr.value().limit);
				l4_vm_vmx_write(vmcs, Vmcs::TR_BASE,  state.tr.value().base);
			}

			if (state.ldtr.valid()) {
				l4_vm_vmx_write(vmcs, Vmcs::LDTR_SEL, state.ldtr.value().sel);
				l4_vm_vmx_write(vmcs, Vmcs::LDTR_AR, _convert_ar(state.ldtr.value().ar));
				l4_vm_vmx_write(vmcs, Vmcs::LDTR_LIMIT, state.ldtr.value().limit);
				l4_vm_vmx_write(vmcs, Vmcs::LDTR_BASE, state.ldtr.value().base);
			}

			if (state.idtr.valid()) {
				l4_vm_vmx_write(vmcs, Vmcs::IDTR_BASE, state.idtr.value().base);
				l4_vm_vmx_write(vmcs, Vmcs::IDTR_LIMIT, state.idtr.value().limit);
			}

			if (state.gdtr.valid()) {
				l4_vm_vmx_write(vmcs, Vmcs::GDTR_BASE, state.gdtr.value().base);
				l4_vm_vmx_write(vmcs, Vmcs::GDTR_LIMIT, state.gdtr.value().limit);
			}

			if (state.pdpte_0.valid() || state.pdpte_1.valid() ||
			    state.pdpte_2.valid() || state.pdpte_3.valid())
			{
				if (_show_error_unsupported_pdpte) {
					_show_error_unsupported_pdpte = false;
					Genode::error("PDPTE 0/1/2/3 not supported on Fiasco.OC");
				}
			}

			if (state.sysenter_cs.valid())
				l4_vm_vmx_write(vmcs, Vmcs::SYSENTER_CS,
				                state.sysenter_cs.value());
			if (state.sysenter_sp.valid())
				l4_vm_vmx_write(vmcs, Vmcs::SYSENTER_SP,
				                state.sysenter_sp.value());
			if (state.sysenter_ip.valid())
				l4_vm_vmx_write(vmcs, Vmcs::SYSENTER_IP,
				                state.sysenter_ip.value());

			if (state.fpu.valid()) {
				if (_show_error_unsupported_fpu) {
					_show_error_unsupported_fpu = false;
					Genode::error("FPU guest state not supported on Fiasco.OC");
				}
			}
		}

		void _write_amd_state(Vm_state &state, Fiasco::l4_vm_svm_vmcb_t *vmcb,
		                      Fiasco::l4_vcpu_state_t *vcpu)
		{
			if (state.ax.valid() || state.cx.valid() || state.dx.valid() ||
			    state.bx.valid()) {

				vmcb->state_save_area.rax = state.ax.value();
				vcpu->r.ax = state.ax.value();
				vcpu->r.cx = state.cx.value();
				vcpu->r.dx = state.dx.value();
				vcpu->r.bx = state.bx.value();
			}

			if (state.bp.valid() || state.di.valid() || state.si.valid()) {
				vcpu->r.bp = state.bp.value();
				vcpu->r.di = state.di.value();
				vcpu->r.si = state.si.value();
			}

			if (state.r8.valid()  || state.r9.valid() || state.r10.valid() ||
			    state.r11.valid() || state.r12.valid() || state.r13.valid() ||
			    state.r14.valid() || state.r15.valid()) {
#ifdef __x86_64__
				vcpu->r.r8 = state.r8.value();
				vcpu->r.r9 = state.r9.value();
				vcpu->r.r10 = state.r10.value();
				vcpu->r.r11 = state.r11.value();
				vcpu->r.r12 = state.r12.value();
				vcpu->r.r13 = state.r13.value();
				vcpu->r.r14 = state.r14.value();
				vcpu->r.r15 = state.r15.value();
#endif
			}

			if (state.tsc_offset.valid()) {
				_tsc_offset += state.tsc_offset.value();
				vmcb->control_area.tsc_offset = _tsc_offset;
			}

			if (state.star.value() || state.lstar.value() ||
			    state.fmask.value() || state.kernel_gs_base.value())
				Genode::error(__LINE__, " not implemented");

			if (state.tpr.valid() || state.tpr_threshold.valid()) {
				if (_show_error_unsupported_tpr) {
					_show_error_unsupported_tpr = false;
					Genode::error("TPR & TPR_THRESHOLD not supported on Fiasco.OC");
				}
			}

			if (state.dr7.valid())
				vmcb->state_save_area.dr7 = state.dr7.value();

			if (state.cr0.valid()) {
				vmcb->state_save_area.cr0 = vmcb_cr0_set | (~vmcb_cr0_mask & state.cr0.value());
				vmcb_cr0_shadow           = state.cr0.value();
#if 0
				vmcb->state_save_area.xcr0 = state.cr0();
#endif
			}

			if (state.cr2.valid())
				vmcb->state_save_area.cr2 = state.cr2.value();

			if (state.cr3.valid())
				vmcb->state_save_area.cr3 = state.cr3.value();

			if (state.cr4.valid()) {
				vmcb->state_save_area.cr4 = vmcb_cr4_set | (~vmcb_cr4_mask & state.cr4.value());
				vmcb_cr4_shadow           = state.cr4.value();
			}

			if (state.ctrl_primary.valid())
				vmcb->control_area.intercept_instruction0 = vmcb_ctrl0 |
				                                            state.ctrl_primary.value();

			if (state.ctrl_secondary.valid())
				vmcb->control_area.intercept_instruction1 = vmcb_ctrl1 |
				                                            state.ctrl_secondary.value();

			if (state.inj_info.valid()) {
				if (state.inj_info.value() & 0x1000) {
					vmcb->control_area.interrupt_ctl |=  (1ul << 8 | 1ul << 20);
					vmcb->control_area.intercept_instruction0 |= Vmcb::CTRL0_VINTR;
				} else {
					vmcb->control_area.interrupt_ctl &= ~(1ul << 8 | 1ul << 20);
					vmcb->control_area.intercept_instruction0 &= ~Vmcb::CTRL0_VINTR;
				}
				vmcb->control_area.eventinj  = 0;
				vmcb->control_area.eventinj |= ~0x3000U & state.inj_info.value();
			}

			if (state.inj_error.valid()) {
				vmcb->control_area.eventinj &= ((1ULL << 32) - 1);
				uint64_t value = (0ULL + state.inj_error.value()) << 32;
				vmcb->control_area.eventinj |= value;
			}

			if (state.flags.valid())
				vmcb->state_save_area.rflags = state.flags.value();

			if (state.sp.valid())
				vmcb->state_save_area.rsp = state.sp.value();

			if (state.ip.valid())
				vmcb->state_save_area.rip = state.ip.value();

			if (state.efer.valid())
				vmcb->state_save_area.efer = state.efer.value() | AMD_SVM_ENABLE;

			if (state.intr_state.valid())
				vmcb->control_area.interrupt_shadow = state.intr_state.value();

			/* state.actv_state.valid() - not required for AMD */

			if (state.cs.valid()) {
				vmcb->state_save_area.cs.selector = state.cs.value().sel;
				vmcb->state_save_area.cs.attrib = state.cs.value().ar;
				vmcb->state_save_area.cs.limit = state.cs.value().limit;
				vmcb->state_save_area.cs.base = state.cs.value().base;
			}

			if (state.ss.valid()) {
				vmcb->state_save_area.ss.selector = state.ss.value().sel;
				vmcb->state_save_area.ss.attrib = state.ss.value().ar;
				vmcb->state_save_area.ss.limit = state.ss.value().limit;
				vmcb->state_save_area.ss.base = state.ss.value().base;
			}

			if (state.es.valid()) {
				vmcb->state_save_area.es.selector = state.es.value().sel;
				vmcb->state_save_area.es.attrib = state.es.value().ar;
				vmcb->state_save_area.es.limit = state.es.value().limit;
				vmcb->state_save_area.es.base = state.es.value().base;
			}

			if (state.ds.valid()) {
				vmcb->state_save_area.ds.selector = state.ds.value().sel;
				vmcb->state_save_area.ds.attrib = state.ds.value().ar;
				vmcb->state_save_area.ds.limit = state.ds.value().limit;
				vmcb->state_save_area.ds.base = state.ds.value().base;
			}

			if (state.fs.valid()) {
				vmcb->state_save_area.fs.selector = state.fs.value().sel;
				vmcb->state_save_area.fs.attrib = state.fs.value().ar;
				vmcb->state_save_area.fs.limit = state.fs.value().limit;
				vmcb->state_save_area.fs.base = state.fs.value().base;
			}

			if (state.gs.valid()) {
				vmcb->state_save_area.gs.selector = state.gs.value().sel;
				vmcb->state_save_area.gs.attrib = state.gs.value().ar;
				vmcb->state_save_area.gs.limit = state.gs.value().limit;
				vmcb->state_save_area.gs.base = state.gs.value().base;
			}

			if (state.tr.valid()) {
				vmcb->state_save_area.tr.selector = state.tr.value().sel;
				vmcb->state_save_area.tr.attrib = state.tr.value().ar;
				vmcb->state_save_area.tr.limit = state.tr.value().limit;
				vmcb->state_save_area.tr.base = state.tr.value().base;
			}

			if (state.ldtr.valid()) {
				vmcb->state_save_area.ldtr.selector = state.ldtr.value().sel;
				vmcb->state_save_area.ldtr.attrib = state.ldtr.value().ar;
				vmcb->state_save_area.ldtr.limit = state.ldtr.value().limit;
				vmcb->state_save_area.ldtr.base = state.ldtr.value().base;
			}

			if (state.idtr.valid()) {
				vmcb->state_save_area.idtr.base = state.idtr.value().base;
				vmcb->state_save_area.idtr.limit = state.idtr.value().limit;
			}

			if (state.gdtr.valid()) {
				vmcb->state_save_area.gdtr.base = state.gdtr.value().base;
				vmcb->state_save_area.gdtr.limit = state.gdtr.value().limit;
			}

			if (state.pdpte_0.valid() || state.pdpte_1.valid() ||
			    state.pdpte_2.valid() || state.pdpte_3.valid())
			{
				if (_show_error_unsupported_pdpte) {
					_show_error_unsupported_pdpte = false;
					Genode::error("PDPTE 0/1/2/3 not supported on Fiasco.OC");
				}
			}

			if (state.sysenter_cs.valid())
				vmcb->state_save_area.sysenter_cs = state.sysenter_cs.value();
			if (state.sysenter_sp.valid())
				vmcb->state_save_area.sysenter_esp = state.sysenter_sp.value();
			if (state.sysenter_ip.valid())
				vmcb->state_save_area.sysenter_eip = state.sysenter_ip.value();

			if (state.fpu.valid()) {
				if (_show_error_unsupported_fpu) {
					_show_error_unsupported_fpu = false;
					Genode::error("FPU guest state not supported on Fiasco.OC");
				}
			}
		}

	public:

		Vcpu(Env &env, Signal_context_capability &cap,
		     Semaphore &handler_ready, enum Virt type,
		     Allocator &alloc, Affinity::Location location)
		:
			Thread(env, "vcpu_thread", STACK_SIZE, location, Weight(), env.cpu()),
			_signal(cap), _handler_ready(handler_ready), _alloc(alloc),
			_vm_type(type)
		{ }

		Allocator &allocator() const { return _alloc; }

		bool match(Vm_session_client::Vcpu_id id) { return id.id == _id.id; }

		Genode::Vm_session_client::Vcpu_id id() const  { return _id; }
		void id(Genode::Vm_session_client::Vcpu_id id) { _id = id;   }

		void assign_ds_state(Region_map &rm, Dataspace_capability cap)
		{
			_state = rm.attach(cap);
			_task  = *reinterpret_cast<Fiasco::l4_cap_idx_t *>(_state);
			*reinterpret_cast<Fiasco::l4_cap_idx_t *>(_state) = 0UL;
		}

		void resume()
		{
			Mutex::Guard guard(_remote_mutex);

			if (_state_request == RUN || _state_request == PAUSE)
				return;

			_state_request = RUN;

			if (_state_current == NONE)
				_wake_up.up();
		}

		void pause()
		{
			Mutex::Guard guard(_remote_mutex);

			if (_state_request == PAUSE)
				return;

			_state_request = PAUSE;

			/* recall vCPU */
			Fiasco::l4_cap_idx_t tid = native_thread().kcap;
			Fiasco::l4_cap_idx_t irq = tid + Fiasco::TASK_VCPU_IRQ_CAP;
			Fiasco::l4_irq_trigger(irq);

			if (_state_current == NONE)
				_wake_up.up();
		}

		void terminate()
		{
			_state_request = TERMINATE;
			_wake_up.up();
		}
};


static enum Virt virt_type(Genode::Env &env)
{
	try {
		Genode::Attached_rom_dataspace const info(env, "platform_info");
		Genode::Xml_node const features = info.xml().sub_node("hardware").sub_node("features");

		if (features.attribute_value("svm", false))
			return Virt::SVM;

		if (features.attribute_value("vmx", false))
			return Virt::VMX;
	} catch (...) { }

	return Virt::UNKNOWN;
}


Vm_session_client::Vcpu_id
Vm_session_client::create_vcpu(Allocator &alloc, Env &env,
                               Vm_handler_base &handler)
{
	enum Virt vm_type = virt_type(env);
	if (vm_type == Virt::UNKNOWN) {
		Genode::error("unsupported hardware virtualisation");
		return Vm_session::Vcpu_id();
	}

	Thread * ep = reinterpret_cast<Thread *>(&handler._rpc_ep);
	Affinity::Location location = ep->affinity();

	/* create thread that switches modes between thread/cpu */
	Vcpu * vcpu = new (alloc) Registered<Vcpu>(vcpus, env, handler._cap,
	                                           handler._done, vm_type,
	                                           alloc, location);

	try {
		/* now it gets actually valid - vcpu->cap() becomes valid */
		vcpu->start();

		/* instruct core to let it become a vCPU */
		vcpu->id(call<Rpc_create_vcpu>(vcpu->cap()));

		call<Rpc_exception_handler>(handler._cap, vcpu->id());

		vcpu->assign_ds_state(env.rm(), call<Rpc_cpu_state>(vcpu->id()));
	} catch (...) {
		vcpu->terminate();
		vcpu->join();

		destroy(alloc, vcpu);
		throw;
	}
	return vcpu->id();
}

void Vm_session_client::run(Vcpu_id vcpu_id)
{
	vcpus.for_each([&] (Vcpu &vcpu) {
		if (vcpu.match(vcpu_id))
			vcpu.resume();
	});
}

void Vm_session_client::pause(Vm_session_client::Vcpu_id vcpu_id)
{
	vcpus.for_each([&] (Vcpu &vcpu) {
		if (!vcpu.match(vcpu_id))
			return;

		vcpu.pause();
	});
}

Dataspace_capability Vm_session_client::cpu_state(Vcpu_id vcpu_id)
{
	Dataspace_capability cap;

	vcpus.for_each([&] (Vcpu &vcpu) {
		if (vcpu.match(vcpu_id))
			cap = call<Rpc_cpu_state>(vcpu_id);
	});

	return cap;
}

Vm_session::~Vm_session()
{
	vcpus.for_each([&] (Vcpu &vc) {
		Allocator &alloc = vc.allocator();
		destroy(alloc, &vc);
	});
}
