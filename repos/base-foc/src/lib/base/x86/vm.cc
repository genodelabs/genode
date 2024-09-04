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
#include <base/attached_rom_dataspace.h>
#include <base/env.h>
#include <base/registry.h>
#include <base/sleep.h>
#include <cpu/vcpu_state.h>
#include <trace/timestamp.h>
#include <vm_session/connection.h>
#include <vm_session/handler.h>

#include <foc_native_vcpu/foc_native_vcpu.h>

/* Fiasco.OC includes */
#include <foc/syscall.h>
#include <foc/native_thread.h>
#include <foc/native_capability.h>

namespace Foc {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
#include <l4/sys/vcpu.h>
#include <l4/sys/__vm-svm.h>
#include <l4/sys/__vm-vmx.h>
#pragma GCC diagnostic pop  /* restore -Wconversion warnings */
}

using namespace Genode;

using Exit_config = Vm_connection::Exit_config;
using Call_with_state = Vm_connection::Call_with_state;


enum Virt { VMX, SVM, UNKNOWN };


static uint32_t svm_features()
{
	uint32_t cpuid = 0x8000000a, edx = 0, ebx = 0, ecx = 0;
	asm volatile ("cpuid" : "+a" (cpuid), "=d" (edx), "=b"(ebx), "=c"(ecx));
	return edx;
}


static bool svm_np() { return svm_features() & (1U << 0); }


/***********************************
 ** Fiasco.OC vCPU implementation **
 ***********************************/

struct Foc_vcpu;

struct Foc_native_vcpu_rpc : Rpc_client<Vm_session::Native_vcpu>, Noncopyable
{
	private:

		Capability<Vm_session::Native_vcpu> _create_vcpu(Vm_connection &vm,
		                                                 Thread_capability &cap)
		{
			return vm.with_upgrade([&] {
				return vm.call<Vm_session::Rpc_create_vcpu>(cap); });
		}

	public:

		Foc_vcpu &vcpu;

		Foc_native_vcpu_rpc(Vm_connection &vm, Thread_capability &cap,
		                    Foc_vcpu &vcpu)
		:
			Rpc_client<Vm_session::Native_vcpu>(_create_vcpu(vm, cap)),
			vcpu(vcpu)
		{ }

		Foc::l4_cap_idx_t task_index() { return call<Rpc_task_index>(); }

		Foc::l4_vcpu_state_t * foc_vcpu_state() {
			return reinterpret_cast<Foc::l4_vcpu_state_t *>(call<Rpc_foc_vcpu_state>()); }
};


struct Foc_vcpu : Thread, Noncopyable
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

			SS_SEL     = 0x0804,
			SS_LIMIT   = 0x4804,
			SS_AR      = 0x4818,
			SS_BASE    = 0x680a,

			ES_SEL     = 0x0800,
			ES_LIMIT   = 0x4800,
			ES_AR      = 0x4814,
			ES_BASE    = 0x6806,

			DS_SEL     = 0x0806,
			DS_LIMIT   = 0x4806,
			DS_AR      = 0x481a,
			DS_BASE    = 0x680c,

			FS_SEL     = 0x0808,
			FS_LIMIT   = 0x4808,
			FS_AR      = 0x481c,
			FS_BASE    = 0x680e,

			GS_SEL     = 0x080a,
			GS_LIMIT   = 0x480a,
			GS_AR      = 0x481e,
			GS_BASE    = 0x6810,

			LDTR_SEL   = 0x080c,
			LDTR_LIMIT = 0x480c,
			LDTR_AR    = 0x4820,
			LDTR_BASE  = 0x6812,

			TR_SEL     = 0x080e,
			TR_LIMIT   = 0x480e,
			TR_AR      = 0x4822,
			TR_BASE    = 0x6814,

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

			IDT_INFO   = 0x4408,
			IDT_ERROR  = 0x440a,

			EXIT_CTRL  = 0x400c,
			ENTRY_CTRL = 0x4012,

			TSC_OFF_LO = 0x2010,
			TSC_OFF_HI = 0x2011,

			MSR_FMASK  = 0x2842,
			MSR_LSTAR  = 0x2844,
			MSR_CSTAR  = 0x2846,
			MSR_STAR   = 0x284a,

			KERNEL_GS_BASE = 0x284c,

			CR4_VMX = 1 << 13,

			INTEL_EXIT_INVALID = 0x21,

		};

		enum Vmcb
		{
			CTRL0_VINTR      = 1u << 4,
			CTRL0_IO         = 1u << 27,
			CTRL0_MSR        = 1u << 28,

			AMD_SVM_ENABLE   = 1 << 12,

			AMD_EXIT_INVALID = 0xfd,
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

		addr_t   const _cr0_mask  { CR0_NM | CR0_CD };
		uint32_t const vmcb_ctrl0 { CTRL0_IO | CTRL0_MSR };
		uint32_t const vmcb_ctrl1 { 0 };

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

		Vcpu_handler_base          &_vcpu_handler;
		Vcpu_handler<Foc_vcpu>      _exit_handler;
		Blockade                    _startup { };
		Semaphore                   _wake_up { 0 };
		uint64_t                    _tsc_offset { 0 };
		enum Virt const             _vm_type;
		bool                        _show_error_unsupported_pdpte { true };
		bool                        _show_error_unsupported_tpr   { true };
		Semaphore                   _state_ready { 0 };
		bool                        _dispatching { false };
		bool                        _extra_dispatch_up { false };
		void                       *_ep_handler    { nullptr };

		Vcpu_state             _vcpu_state __attribute__((aligned(0x10))) { };
		Vcpu_state::Fpu::State _fpu_ep     __attribute__((aligned(0x10))) { };
		Vcpu_state::Fpu::State _fpu_vcpu   __attribute__((aligned(0x10))) { };

		Constructible<Foc_native_vcpu_rpc> _rpc   { };

		enum {
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
			/* trigger that thread is up */
			_startup.wakeup();

			/* wait until vcpu is assigned to us */
			_wake_up.down();

			{
				Mutex::Guard guard(_remote_mutex);

				/* leave scope for Thread::join() - vCPU setup failed */
				if (_state_request == TERMINATE)
					return;

				_state_request = NONE;
			}

			Foc::l4_vcpu_state_t * const vcpu = _rpc->foc_vcpu_state();
			addr_t const vcpu_addr = reinterpret_cast<addr_t>(vcpu);

			if (!l4_vcpu_check_version(vcpu))
				error("vCPU version mismatch kernel vs user-land - ",
				      vcpu->version, "!=",
				      (int)Foc::L4_VCPU_STATE_VERSION);

			using namespace Foc;
			l4_vm_svm_vmcb_t *vmcb = reinterpret_cast<l4_vm_svm_vmcb_t *>(vcpu_addr + L4_VCPU_OFFSET_EXT_STATE);
			void * vmcs = reinterpret_cast<void *>(vcpu_addr + L4_VCPU_OFFSET_EXT_STATE);

			/* set vm page table */
			vcpu->user_task = _rpc->task_index();

			Vcpu_state &state = _vcpu_state;
			state.discharge();

			/* initial startup VM exit to get valid VM state */
			if (_vm_type == Virt::VMX)
				_read_intel_state(state, vmcs, vcpu);

			if (_vm_type == Virt::SVM)
				_read_amd_state(state, vmcb, vcpu);

			state.exit_reason = VMEXIT_STARTUP;
			_state_ready.up();
			Signal_transmitter(_exit_handler.signal_cap()).submit();

			_exit_handler.ready_semaphore().down();
			_wake_up.down();

			/*
			 * Fiasoc.OC peculiarities
			 */
			if (_vm_type == Virt::SVM)
				state.efer.charge(state.efer.value() | AMD_SVM_ENABLE);

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
				Foc::l4_vm_vmx_write(vmcs, Vmcs::CR0_MASK, vmcs_cr0_mask);
				Foc::l4_vm_vmx_write(vmcs, Vmcs::CR4_MASK, vmcs_cr4_mask);
				Foc::l4_vm_vmx_write(vmcs, Vmcs::CR4_SHADOW, 0);
				state.cr4.charge(vmcs_cr4_set);

				enum {
					EXIT_SAVE_EFER = 1U << 20,
					ENTRY_LOAD_EFER = 1U << 15,
				};
				Foc::l4_vm_vmx_write(vmcs, Vmcs::EXIT_CTRL, EXIT_SAVE_EFER);
				Foc::l4_vm_vmx_write(vmcs, Vmcs::ENTRY_CTRL, ENTRY_LOAD_EFER);
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
					error("unknown vcpu state ", (int)_state_current);
					while (true) { _remote_mutex.acquire(); }
				}

				/* transfer vCPU state to Fiasco.OC */
				if (_vm_type == Virt::SVM)
					_write_amd_state(state, vmcb, vcpu);
				if (_vm_type == Virt::VMX)
					_write_intel_state(state, vmcs, vcpu);

				/* save FPU state of this thread and restore state of vCPU */
				asm volatile ("fxsave %0" : "=m" (_fpu_ep));
				if (state.fpu.charged()) {
					state.fpu.charge([&] (Vcpu_state::Fpu::State &fpu) {
						asm volatile ("fxrstor %0" : : "m" (fpu) : "memory");
						return 512;
					});
				} else
					asm volatile ("fxrstor %0" : : "m" (_fpu_vcpu) : "memory");

				/* tell Fiasco.OC to run the vCPU */
				l4_msgtag_t tag = l4_thread_vcpu_resume_start();
				tag = l4_thread_vcpu_resume_commit(L4_INVALID_CAP, tag);

				/* save FPU state of vCPU and restore state of this thread */
				state.fpu.charge([&] (Vcpu_state::Fpu::State &fpu) {
					asm volatile ("fxsave %0" : "=m" (fpu) :: "memory");
					asm volatile ("fxsave %0" : "=m" (_fpu_vcpu) :: "memory");
					return 512;
				});
				asm volatile ("fxrstor %0" : : "m" (_fpu_ep) : "memory");

				/* got VM exit or interrupted by asynchronous signal */
				uint64_t reason = 0;

				state.discharge();

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
								Foc::l4_cap_idx_t tid = native_thread().kcap;
								Foc::l4_cap_idx_t irq = tid + Foc::TASK_VCPU_IRQ_CAP;
								l4_irq_receive(irq, L4_IPC_RECV_TIMEOUT_0);
							}
						}
					}

					state.exit_reason = reason & 0xff;
					_read_amd_state(state, vmcb, vcpu);
				}

				if (_vm_type == Virt::VMX) {
					reason = Foc::l4_vm_vmx_read_32(vmcs, Vmcs::EXI_REASON);

					{
						Mutex::Guard guard(_remote_mutex);
						_state_request = NONE;
						_state_current = PAUSE;

						/* remotely PAUSE was called */
						if (l4_error(tag) && reason == 0x1) {
							reason = VMEXIT_PAUSED;

							/* consume notification */
							while (vcpu->sticky_flags) {
								Foc::l4_cap_idx_t tid = native_thread().kcap;
								Foc::l4_cap_idx_t irq = tid + Foc::TASK_VCPU_IRQ_CAP;
								l4_irq_receive(irq, L4_IPC_RECV_TIMEOUT_0);
							}
						}
					}

					state.exit_reason = reason & 0xff;
					_read_intel_state(state, vmcs, vcpu);
				}

				_state_ready.up();

				/*
				 * If the handler is run because the L4 IRQ triggered a
				 * VMEXIT_PAUSED, the signal handler has already been dispatched
				 * asynchronously and is waiting for the _state_ready semaphore
				 * to come up. In that case wrap around the loop to continue
				 * without another signal.
				 *
				 * If the async signal handler has been queued while a regular
				 * exit was pending, the regular exit may be processed by the
				 * async handler with the exit signal handler running
				 * afterwards and this vCPU loop waiting for the exit signal
				 * handler to finish.
				 * In this case, the with_state() method does an extra up()
				 * on the _exit_handler.ready_semaphore() to cause delivery
				 * of the VMEXIT_PAUSED signal to the regular exit signal
				 * handler in the next run of the loop.
				 * Once the signal has been delivered, (_state_ready.up()), the
				 * extra semaphore up has to be countered by an additional
				 * down().
				 * This down() will wait for the exit signal handler to finish
				 * processing the VMEXIT_PAUSED before the loop is continued.
				 */
				if (reason == VMEXIT_PAUSED) {
					if (_extra_dispatch_up) {
						_extra_dispatch_up = false;
						_exit_handler.ready_semaphore().down();
					}
					continue;
				}

				/* notify VM handler */
				Signal_transmitter(_exit_handler.signal_cap()).submit();

				/*
				 * Wait until VM handler is really really done,
				 * otherwise we lose state.
				 */
				_exit_handler.ready_semaphore().down();
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
			return (uint16_t)(((value & 0x1f000) >> 4) | (value & 0xff)); }

		void _read_intel_state(Vcpu_state &state, void *vmcs,
		                       Foc::l4_vcpu_state_t *vcpu)
		{
			state.ax.charge(vcpu->r.ax);
			state.cx.charge(vcpu->r.cx);
			state.dx.charge(vcpu->r.dx);
			state.bx.charge(vcpu->r.bx);

			state.bp.charge(vcpu->r.bp);
			state.di.charge(vcpu->r.di);
			state.si.charge(vcpu->r.si);

			state.flags .charge((addr_t)Foc::l4_vm_vmx_read(vmcs, Vmcs::FLAGS));
			state.sp    .charge((addr_t)Foc::l4_vm_vmx_read(vmcs, Vmcs::SP));
			state.ip    .charge((addr_t)Foc::l4_vm_vmx_read(vmcs, Vmcs::IP));
			state.ip_len.charge((addr_t)Foc::l4_vm_vmx_read(vmcs, Vmcs::INST_LEN));
			state.dr7   .charge((addr_t)Foc::l4_vm_vmx_read(vmcs, Vmcs::DR7));

#ifdef __x86_64__
			state.r8 .charge(vcpu->r.r8);
			state.r9 .charge(vcpu->r.r9);
			state.r10.charge(vcpu->r.r10);
			state.r11.charge(vcpu->r.r11);
			state.r12.charge(vcpu->r.r12);
			state.r13.charge(vcpu->r.r13);
			state.r14.charge(vcpu->r.r14);
			state.r15.charge(vcpu->r.r15);
#endif

			{
				addr_t const cr0        = (addr_t)Foc::l4_vm_vmx_read(vmcs, Vmcs::CR0);
				addr_t const cr0_shadow = (addr_t)Foc::l4_vm_vmx_read(vmcs, Vmcs::CR0_SHADOW);

				state.cr0.charge((cr0 & ~vmcs_cr0_mask) | (cr0_shadow & vmcs_cr0_mask));

				if (state.cr0.value() != cr0_shadow)
					Foc::l4_vm_vmx_write(vmcs, Vmcs::CR0_SHADOW, state.cr0.value());
			}

			unsigned const cr2 = Foc::l4_vm_vmx_get_cr2_index(vmcs);
			state.cr2.charge((addr_t)Foc::l4_vm_vmx_read(vmcs, cr2));
			state.cr3.charge((addr_t)Foc::l4_vm_vmx_read(vmcs, Vmcs::CR3));

			{
				addr_t const cr4        = (addr_t)Foc::l4_vm_vmx_read(vmcs, Vmcs::CR4);
				addr_t const cr4_shadow = (addr_t)Foc::l4_vm_vmx_read(vmcs, Vmcs::CR4_SHADOW);

				state.cr4.charge((cr4 & ~vmcs_cr4_mask) | (cr4_shadow & vmcs_cr4_mask));

				if (state.cr4.value() != cr4_shadow)
					Foc::l4_vm_vmx_write(vmcs, Vmcs::CR4_SHADOW,
					                        state.cr4.value());
			}

			using Foc::l4_vm_vmx_read;
			using Foc::l4_vm_vmx_read_16;
			using Foc::l4_vm_vmx_read_32;
			using Foc::l4_vm_vmx_read_nat;
			using Segment = Vcpu_state::Segment;
			using Range   = Vcpu_state::Range;

			{
				Segment cs { l4_vm_vmx_read_16(vmcs, Vmcs::CS_SEL),
				             _convert_ar_16((addr_t)l4_vm_vmx_read(vmcs, Vmcs::CS_AR)),
				             l4_vm_vmx_read_32(vmcs, Vmcs::CS_LIMIT),
				             l4_vm_vmx_read_nat(vmcs, Vmcs::CS_BASE) };

				state.cs.charge(cs);
			}

			{
				Segment ss { l4_vm_vmx_read_16(vmcs, Vmcs::SS_SEL),
				            _convert_ar_16((addr_t)l4_vm_vmx_read(vmcs, Vmcs::SS_AR)),
				            l4_vm_vmx_read_32(vmcs, Vmcs::SS_LIMIT),
				            l4_vm_vmx_read_nat(vmcs, Vmcs::SS_BASE) };

				state.ss.charge(ss);
			}

			{
				Segment es { l4_vm_vmx_read_16(vmcs, Vmcs::ES_SEL),
				             _convert_ar_16((addr_t)l4_vm_vmx_read(vmcs, Vmcs::ES_AR)),
				             l4_vm_vmx_read_32(vmcs, Vmcs::ES_LIMIT),
				             l4_vm_vmx_read_nat(vmcs, Vmcs::ES_BASE) };

				state.es.charge(es);
			}

			{
				Segment ds { l4_vm_vmx_read_16(vmcs, Vmcs::DS_SEL),
				             _convert_ar_16((addr_t)l4_vm_vmx_read(vmcs, Vmcs::DS_AR)),
				             l4_vm_vmx_read_32(vmcs, Vmcs::DS_LIMIT),
				             l4_vm_vmx_read_nat(vmcs, Vmcs::DS_BASE) };

				state.ds.charge(ds);
			}

			{
				Segment fs { l4_vm_vmx_read_16(vmcs, Vmcs::FS_SEL),
				             _convert_ar_16((addr_t)l4_vm_vmx_read(vmcs, Vmcs::FS_AR)),
				             l4_vm_vmx_read_32(vmcs, Vmcs::FS_LIMIT),
				             l4_vm_vmx_read_nat(vmcs, Vmcs::FS_BASE) };

				state.fs.charge(fs);
			}

			{
				Segment gs { l4_vm_vmx_read_16(vmcs, Vmcs::GS_SEL),
				             _convert_ar_16((addr_t)l4_vm_vmx_read(vmcs, Vmcs::GS_AR)),
				             l4_vm_vmx_read_32(vmcs, Vmcs::GS_LIMIT),
				             l4_vm_vmx_read_nat(vmcs, Vmcs::GS_BASE) };

				state.gs.charge(gs);
			}

			{
				Segment tr { l4_vm_vmx_read_16(vmcs, Vmcs::TR_SEL),
				             _convert_ar_16((addr_t)l4_vm_vmx_read(vmcs, Vmcs::TR_AR)),
				             l4_vm_vmx_read_32(vmcs, Vmcs::TR_LIMIT),
				             l4_vm_vmx_read_nat(vmcs, Vmcs::TR_BASE) };

				state.tr.charge(tr);
			}

			{
				Segment ldtr { l4_vm_vmx_read_16(vmcs, Vmcs::LDTR_SEL),
				               _convert_ar_16((addr_t)l4_vm_vmx_read(vmcs, Vmcs::LDTR_AR)),
				               l4_vm_vmx_read_32(vmcs, Vmcs::LDTR_LIMIT),
				               l4_vm_vmx_read_nat(vmcs, Vmcs::LDTR_BASE) };

				state.ldtr.charge(ldtr);
			}

			state.gdtr.charge(Range{.limit = l4_vm_vmx_read_32(vmcs, Vmcs::GDTR_LIMIT),
			                        .base  = l4_vm_vmx_read_nat(vmcs, Vmcs::GDTR_BASE)});

			state.idtr.charge(Range{.limit = l4_vm_vmx_read_32(vmcs, Vmcs::IDTR_LIMIT),
			                        .base  = l4_vm_vmx_read_nat(vmcs, Vmcs::IDTR_BASE)});

			state.sysenter_cs.charge((addr_t)l4_vm_vmx_read(vmcs, Vmcs::SYSENTER_CS));
			state.sysenter_sp.charge((addr_t)l4_vm_vmx_read(vmcs, Vmcs::SYSENTER_SP));
			state.sysenter_ip.charge((addr_t)l4_vm_vmx_read(vmcs, Vmcs::SYSENTER_IP));

			state.qual_primary  .charge(l4_vm_vmx_read(vmcs, Vmcs::EXIT_QUAL));
			state.qual_secondary.charge(l4_vm_vmx_read(vmcs, Vmcs::GUEST_PHYS));

			state.ctrl_primary  .charge((uint32_t)l4_vm_vmx_read(vmcs, Vmcs::CTRL_0));
			state.ctrl_secondary.charge((uint32_t)l4_vm_vmx_read(vmcs, Vmcs::CTRL_1));

			if (state.exit_reason == INTEL_EXIT_INVALID ||
			    state.exit_reason == VMEXIT_PAUSED)
			{
				state.inj_info .charge((uint32_t)l4_vm_vmx_read(vmcs, Vmcs::INTR_INFO));
				state.inj_error.charge((uint32_t)l4_vm_vmx_read(vmcs, Vmcs::INTR_ERROR));
			} else {
				state.inj_info .charge((uint32_t)l4_vm_vmx_read(vmcs, Vmcs::IDT_INFO));
				state.inj_error.charge((uint32_t)l4_vm_vmx_read(vmcs, Vmcs::IDT_ERROR));
			}

			state.intr_state.charge((uint32_t)l4_vm_vmx_read(vmcs, Vmcs::STATE_INTR));
			state.actv_state.charge((uint32_t)l4_vm_vmx_read(vmcs, Vmcs::STATE_ACTV));

			state.tsc.charge(Trace::timestamp());
			state.tsc_offset.charge(_tsc_offset);

			state.efer.charge((addr_t)l4_vm_vmx_read(vmcs, Vmcs::EFER));

			state.star .charge(l4_vm_vmx_read(vmcs, Vmcs::MSR_STAR));
			state.lstar.charge(l4_vm_vmx_read(vmcs, Vmcs::MSR_LSTAR));
			state.cstar.charge(l4_vm_vmx_read(vmcs, Vmcs::MSR_CSTAR));
			state.fmask.charge(l4_vm_vmx_read(vmcs, Vmcs::MSR_FMASK));
			state.kernel_gs_base.charge(l4_vm_vmx_read(vmcs, Vmcs::KERNEL_GS_BASE));

			/* XXX missing */
#if 0
			if (state.pdpte_0_updated() || state.pdpte_1_updated() ||
			if (state.tpr_updated() || state.tpr_threshold_updated()) {
#endif
		}

		void _read_amd_state(Vcpu_state &state, Foc::l4_vm_svm_vmcb_t *vmcb,
		                     Foc::l4_vcpu_state_t * const vcpu)
		{
			state.ax.charge((addr_t)vmcb->state_save_area.rax);
			state.cx.charge(vcpu->r.cx);
			state.dx.charge(vcpu->r.dx);
			state.bx.charge(vcpu->r.bx);

			state.di.charge(vcpu->r.di);
			state.si.charge(vcpu->r.si);
			state.bp.charge(vcpu->r.bp);

			state.flags.charge((addr_t)vmcb->state_save_area.rflags);

			state.sp.charge((addr_t)vmcb->state_save_area.rsp);

			state.ip.charge((addr_t)vmcb->state_save_area.rip);
			state.ip_len.charge(0); /* unsupported on AMD */

			state.dr7.charge((addr_t)vmcb->state_save_area.dr7);

#ifdef __x86_64__
			state.r8 .charge(vcpu->r.r8);
			state.r9 .charge(vcpu->r.r9);
			state.r10.charge(vcpu->r.r10);
			state.r11.charge(vcpu->r.r11);
			state.r12.charge(vcpu->r.r12);
			state.r13.charge(vcpu->r.r13);
			state.r14.charge(vcpu->r.r14);
			state.r15.charge(vcpu->r.r15);
#endif

			{
				addr_t const cr0 = (addr_t)vmcb->state_save_area.cr0;
				state.cr0.charge((cr0 & ~vmcb_cr0_mask) | (vmcb_cr0_shadow & vmcb_cr0_mask));
				if (state.cr0.value() != vmcb_cr0_shadow)
					vmcb_cr0_shadow = state.cr0.value();
			}
			state.cr2.charge((addr_t)vmcb->state_save_area.cr2);
			state.cr3.charge((addr_t)vmcb->state_save_area.cr3);
			{
				addr_t const cr4 = (addr_t)vmcb->state_save_area.cr4;
				state.cr4.charge((cr4 & ~vmcb_cr4_mask) | (vmcb_cr4_shadow & vmcb_cr4_mask));
				if (state.cr4.value() != vmcb_cr4_shadow)
					vmcb_cr4_shadow = state.cr4.value();
			}

			using Segment = Vcpu_state::Segment;

			state.cs.charge(Segment{vmcb->state_save_area.cs.selector,
			                        vmcb->state_save_area.cs.attrib,
			                        vmcb->state_save_area.cs.limit,
			                        (addr_t)vmcb->state_save_area.cs.base});

			state.ss.charge(Segment{vmcb->state_save_area.ss.selector,
			                        vmcb->state_save_area.ss.attrib,
			                        vmcb->state_save_area.ss.limit,
			                        (addr_t)vmcb->state_save_area.ss.base});

			state.es.charge(Segment{vmcb->state_save_area.es.selector,
			                        vmcb->state_save_area.es.attrib,
			                        vmcb->state_save_area.es.limit,
			                        (addr_t)vmcb->state_save_area.es.base});

			state.ds.charge(Segment{vmcb->state_save_area.ds.selector,
			                        vmcb->state_save_area.ds.attrib,
			                        vmcb->state_save_area.ds.limit,
			                        (addr_t)vmcb->state_save_area.ds.base});

			state.fs.charge(Segment{vmcb->state_save_area.fs.selector,
			                        vmcb->state_save_area.fs.attrib,
			                        vmcb->state_save_area.fs.limit,
			                        (addr_t)vmcb->state_save_area.fs.base});

			state.gs.charge(Segment{vmcb->state_save_area.gs.selector,
			                        vmcb->state_save_area.gs.attrib,
			                        vmcb->state_save_area.gs.limit,
			                        (addr_t)vmcb->state_save_area.gs.base});

			state.tr.charge(Segment{vmcb->state_save_area.tr.selector,
			                        vmcb->state_save_area.tr.attrib,
			                        vmcb->state_save_area.tr.limit,
			                        (addr_t)vmcb->state_save_area.tr.base});

			state.ldtr.charge(Segment{vmcb->state_save_area.ldtr.selector,
			                          vmcb->state_save_area.ldtr.attrib,
			                          vmcb->state_save_area.ldtr.limit,
			                          (addr_t)vmcb->state_save_area.ldtr.base});

			using Range = Vcpu_state::Range;

			state.gdtr.charge(Range{.limit = vmcb->state_save_area.gdtr.limit,
			                        .base  = (addr_t)vmcb->state_save_area.gdtr.base });

			state.idtr.charge(Range{.limit = vmcb->state_save_area.idtr.limit,
			                        .base  = (addr_t)vmcb->state_save_area.idtr.base });

			state.sysenter_cs.charge((addr_t)vmcb->state_save_area.sysenter_cs);
			state.sysenter_sp.charge((addr_t)vmcb->state_save_area.sysenter_esp);
			state.sysenter_ip.charge((addr_t)vmcb->state_save_area.sysenter_eip);

			state.qual_primary  .charge(vmcb->control_area.exitinfo1);
			state.qual_secondary.charge(vmcb->control_area.exitinfo2);

			uint32_t inj_info = 0;
			uint32_t inj_error = 0;
			if (state.exit_reason == AMD_EXIT_INVALID ||
			    state.exit_reason == VMEXIT_PAUSED)
			{
				inj_info  = (uint32_t)vmcb->control_area.eventinj;
				inj_error = (uint32_t)(vmcb->control_area.eventinj >> 32);
			} else {
				inj_info  = (uint32_t)vmcb->control_area.exitintinfo;
				inj_error = (uint32_t)(vmcb->control_area.exitintinfo >> 32);
			}
			state.inj_info .charge(inj_info);
			state.inj_error.charge(inj_error);

			state.intr_state.charge((uint32_t)vmcb->control_area.interrupt_shadow);
			state.actv_state.charge(0);

			state.tsc.charge(Trace::timestamp());
			state.tsc_offset.charge(_tsc_offset);

			state.efer.charge((addr_t)vmcb->state_save_area.efer);

			if (state.pdpte_0.charged() || state.pdpte_1.charged() ||
			    state.pdpte_2.charged() || state.pdpte_3.charged()) {

				error("pdpte not implemented");
			}

			if (state.star.charged() || state.lstar.charged() || state.cstar.charged() ||
			    state.fmask.charged() || state.kernel_gs_base.charged()) {

				error("star, fstar, fmask, kernel_gs_base not implemented");
			}

			if (state.tpr.charged() || state.tpr_threshold.charged()) {
				error("tpr not implemented");
			}
		}

		void _write_intel_state(Vcpu_state &state, void *vmcs,
		                        Foc::l4_vcpu_state_t *vcpu)
		{
			using Foc::l4_vm_vmx_write;

			if (state.ax.charged() || state.cx.charged() || state.dx.charged() ||
			    state.bx.charged()) {
				vcpu->r.ax = state.ax.value();
				vcpu->r.cx = state.cx.value();
				vcpu->r.dx = state.dx.value();
				vcpu->r.bx = state.bx.value();
			}

			if (state.bp.charged() || state.di.charged() || state.si.charged()) {
				vcpu->r.bp = state.bp.value();
				vcpu->r.di = state.di.value();
				vcpu->r.si = state.si.value();
			}

			if (state.r8.charged()  || state.r9.charged() || state.r10.charged() ||
			    state.r11.charged() || state.r12.charged() || state.r13.charged() ||
			    state.r14.charged() || state.r15.charged()) {
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

			if (state.tsc_offset.charged()) {
				_tsc_offset += state.tsc_offset.value();
				l4_vm_vmx_write(vmcs, Vmcs::TSC_OFF_LO,  _tsc_offset & 0xffffffffu);
				l4_vm_vmx_write(vmcs, Vmcs::TSC_OFF_HI, (_tsc_offset >> 32) & 0xffffffffu);
			}

			if (state.star.charged())
				l4_vm_vmx_write(vmcs, Vmcs::MSR_STAR, state.star.value());

			if (state.lstar.charged())
				l4_vm_vmx_write(vmcs, Vmcs::MSR_LSTAR, state.lstar.value());

			if (state.cstar.charged())
				l4_vm_vmx_write(vmcs, Vmcs::MSR_CSTAR, state.cstar.value());

			if (state.fmask.charged())
				l4_vm_vmx_write(vmcs, Vmcs::MSR_FMASK, state.fmask.value());

			if (state.kernel_gs_base.charged())
				l4_vm_vmx_write(vmcs, Vmcs::KERNEL_GS_BASE, state.kernel_gs_base.value());

			if (state.tpr.charged() || state.tpr_threshold.charged()) {
				if (_show_error_unsupported_tpr) {
					_show_error_unsupported_tpr = false;
					error("TPR & TPR_THRESHOLD not supported on Fiasco.OC");
				}
			}

			if (state.dr7.charged())
				l4_vm_vmx_write(vmcs, Vmcs::DR7, state.dr7.value());

			if (state.cr0.charged()) {
				l4_vm_vmx_write(vmcs, Vmcs::CR0, vmcs_cr0_set | (~vmcs_cr0_mask & state.cr0.value()));
				l4_vm_vmx_write(vmcs, Vmcs::CR0_SHADOW, state.cr0.value());

				#if 0 /* if CPU has xsave feature bit ... see Vm::load_guest_xcr0 */
				l4_vm_vmx_write(vmcs, Foc::L4_VM_VMX_VMCS_XCR0, state.cr0.value());
				#endif
			}

			if (state.cr2.charged()) {
				unsigned const cr2 = Foc::l4_vm_vmx_get_cr2_index(vmcs);
				l4_vm_vmx_write(vmcs, cr2, state.cr2.value());
			}

			if (state.cr3.charged())
				l4_vm_vmx_write(vmcs, Vmcs::CR3, state.cr3.value());

			if (state.cr4.charged()) {
				l4_vm_vmx_write(vmcs, Vmcs::CR4,
				                vmcs_cr4_set | (~vmcs_cr4_mask & state.cr4.value()));
				l4_vm_vmx_write(vmcs, Vmcs::CR4_SHADOW, state.cr4.value());
			}

			if (state.inj_info.charged() || state.inj_error.charged()) {
				uint32_t ctrl_0 = state.ctrl_primary.charged()
				              ? (uint32_t)state.ctrl_primary.value()
				              : (uint32_t)Foc::l4_vm_vmx_read(vmcs, Vmcs::CTRL_0);

				if (state.inj_info.value() & 0x2000)
					warning("unimplemented ", state.inj_info.value() & 0x1000, " ",
					                          state.inj_info.value() & 0x2000, " ",
					                          Hex(ctrl_0), " ",
					                          Hex(state.ctrl_secondary.value()));

				if (state.inj_info.value() & 0x1000)
					ctrl_0 |= Vmcs::IRQ_WINDOW;
				else
					ctrl_0 &= ~Vmcs::IRQ_WINDOW;

				state.ctrl_primary.charge(ctrl_0);

				l4_vm_vmx_write(vmcs, Vmcs::INTR_INFO,
				                      state.inj_info.value() & ~0x3000);
				l4_vm_vmx_write(vmcs, Vmcs::INTR_ERROR,
				                      state.inj_error.value());
			}

			if (state.flags.charged())
				l4_vm_vmx_write(vmcs, Vmcs::FLAGS, state.flags.value());

			if (state.sp.charged())
				l4_vm_vmx_write(vmcs, Vmcs::SP, state.sp.value());

			if (state.ip.charged())
				l4_vm_vmx_write(vmcs, Vmcs::IP, state.ip.value());

			if (state.ip_len.charged())
				l4_vm_vmx_write(vmcs, Vmcs::ENTRY_INST_LEN, state.ip_len.value());

			if (state.efer.charged())
				l4_vm_vmx_write(vmcs, Vmcs::EFER, state.efer.value());

			if (state.ctrl_primary.charged())
				l4_vm_vmx_write(vmcs, Vmcs::CTRL_0,
				                _vmcs_ctrl0 | state.ctrl_primary.value());

			if (state.ctrl_secondary.charged())
				l4_vm_vmx_write(vmcs, Vmcs::CTRL_1,
				                state.ctrl_secondary.value());

			if (state.intr_state.charged())
				l4_vm_vmx_write(vmcs, Vmcs::STATE_INTR,
				                state.intr_state.value());

			if (state.actv_state.charged())
				l4_vm_vmx_write(vmcs, Vmcs::STATE_ACTV,
				                state.actv_state.value());

			if (state.cs.charged()) {
				l4_vm_vmx_write(vmcs, Vmcs::CS_SEL, state.cs.value().sel);
				l4_vm_vmx_write(vmcs, Vmcs::CS_AR, _convert_ar(state.cs.value().ar));
				l4_vm_vmx_write(vmcs, Vmcs::CS_LIMIT, state.cs.value().limit);
				l4_vm_vmx_write(vmcs, Vmcs::CS_BASE, state.cs.value().base);
			}

			if (state.ss.charged()) {
				l4_vm_vmx_write(vmcs, Vmcs::SS_SEL, state.ss.value().sel);
				l4_vm_vmx_write(vmcs, Vmcs::SS_AR, _convert_ar(state.ss.value().ar));
				l4_vm_vmx_write(vmcs, Vmcs::SS_LIMIT, state.ss.value().limit);
				l4_vm_vmx_write(vmcs, Vmcs::SS_BASE, state.ss.value().base);
			}

			if (state.es.charged()) {
				l4_vm_vmx_write(vmcs, Vmcs::ES_SEL, state.es.value().sel);
				l4_vm_vmx_write(vmcs, Vmcs::ES_AR, _convert_ar(state.es.value().ar));
				l4_vm_vmx_write(vmcs, Vmcs::ES_LIMIT, state.es.value().limit);
				l4_vm_vmx_write(vmcs, Vmcs::ES_BASE, state.es.value().base);
			}

			if (state.ds.charged()) {
				l4_vm_vmx_write(vmcs, Vmcs::DS_SEL, state.ds.value().sel);
				l4_vm_vmx_write(vmcs, Vmcs::DS_AR, _convert_ar(state.ds.value().ar));
				l4_vm_vmx_write(vmcs, Vmcs::DS_LIMIT, state.ds.value().limit);
				l4_vm_vmx_write(vmcs, Vmcs::DS_BASE, state.ds.value().base);
			}

			if (state.fs.charged()) {
				l4_vm_vmx_write(vmcs, Vmcs::FS_SEL, state.fs.value().sel);
				l4_vm_vmx_write(vmcs, Vmcs::FS_AR, _convert_ar(state.fs.value().ar));
				l4_vm_vmx_write(vmcs, Vmcs::FS_LIMIT, state.fs.value().limit);
				l4_vm_vmx_write(vmcs, Vmcs::FS_BASE, state.fs.value().base);
			}

			if (state.gs.charged()) {
				l4_vm_vmx_write(vmcs, Vmcs::GS_SEL, state.gs.value().sel);
				l4_vm_vmx_write(vmcs, Vmcs::GS_AR, _convert_ar(state.gs.value().ar));
				l4_vm_vmx_write(vmcs, Vmcs::GS_LIMIT, state.gs.value().limit);
				l4_vm_vmx_write(vmcs, Vmcs::GS_BASE, state.gs.value().base);
			}

			if (state.tr.charged()) {
				l4_vm_vmx_write(vmcs, Vmcs::TR_SEL, state.tr.value().sel);
				l4_vm_vmx_write(vmcs, Vmcs::TR_AR, _convert_ar(state.tr.value().ar));
				l4_vm_vmx_write(vmcs, Vmcs::TR_LIMIT, state.tr.value().limit);
				l4_vm_vmx_write(vmcs, Vmcs::TR_BASE,  state.tr.value().base);
			}

			if (state.ldtr.charged()) {
				l4_vm_vmx_write(vmcs, Vmcs::LDTR_SEL, state.ldtr.value().sel);
				l4_vm_vmx_write(vmcs, Vmcs::LDTR_AR, _convert_ar(state.ldtr.value().ar));
				l4_vm_vmx_write(vmcs, Vmcs::LDTR_LIMIT, state.ldtr.value().limit);
				l4_vm_vmx_write(vmcs, Vmcs::LDTR_BASE, state.ldtr.value().base);
			}

			if (state.idtr.charged()) {
				l4_vm_vmx_write(vmcs, Vmcs::IDTR_BASE, state.idtr.value().base);
				l4_vm_vmx_write(vmcs, Vmcs::IDTR_LIMIT, state.idtr.value().limit);
			}

			if (state.gdtr.charged()) {
				l4_vm_vmx_write(vmcs, Vmcs::GDTR_BASE, state.gdtr.value().base);
				l4_vm_vmx_write(vmcs, Vmcs::GDTR_LIMIT, state.gdtr.value().limit);
			}

			if (state.pdpte_0.charged() || state.pdpte_1.charged() ||
			    state.pdpte_2.charged() || state.pdpte_3.charged())
			{
				if (_show_error_unsupported_pdpte) {
					_show_error_unsupported_pdpte = false;
					error("PDPTE 0/1/2/3 not supported on Fiasco.OC");
				}
			}

			if (state.sysenter_cs.charged())
				l4_vm_vmx_write(vmcs, Vmcs::SYSENTER_CS,
				                state.sysenter_cs.value());
			if (state.sysenter_sp.charged())
				l4_vm_vmx_write(vmcs, Vmcs::SYSENTER_SP,
				                state.sysenter_sp.value());
			if (state.sysenter_ip.charged())
				l4_vm_vmx_write(vmcs, Vmcs::SYSENTER_IP,
				                state.sysenter_ip.value());
		}

		void _write_amd_state(Vcpu_state &state, Foc::l4_vm_svm_vmcb_t *vmcb,
		                      Foc::l4_vcpu_state_t *vcpu)
		{
			if (state.ax.charged() || state.cx.charged() || state.dx.charged() ||
			    state.bx.charged()) {

				vmcb->state_save_area.rax = state.ax.value();
				vcpu->r.ax = state.ax.value();
				vcpu->r.cx = state.cx.value();
				vcpu->r.dx = state.dx.value();
				vcpu->r.bx = state.bx.value();
			}

			if (state.bp.charged() || state.di.charged() || state.si.charged()) {
				vcpu->r.bp = state.bp.value();
				vcpu->r.di = state.di.value();
				vcpu->r.si = state.si.value();
			}

			if (state.r8.charged()  || state.r9.charged() || state.r10.charged() ||
			    state.r11.charged() || state.r12.charged() || state.r13.charged() ||
			    state.r14.charged() || state.r15.charged()) {
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

			if (state.tsc_offset.charged()) {
				_tsc_offset += state.tsc_offset.value();
				vmcb->control_area.tsc_offset = _tsc_offset;
			}

			if (state.star.value() || state.lstar.value() || state.cstar.value() ||
			    state.fmask.value() || state.kernel_gs_base.value())
				error(__LINE__, " not implemented");

			if (state.tpr.charged() || state.tpr_threshold.charged()) {
				if (_show_error_unsupported_tpr) {
					_show_error_unsupported_tpr = false;
					error("TPR & TPR_THRESHOLD not supported on Fiasco.OC");
				}
			}

			if (state.dr7.charged())
				vmcb->state_save_area.dr7 = state.dr7.value();

			if (state.cr0.charged()) {
				vmcb->state_save_area.cr0 = vmcb_cr0_set | (~vmcb_cr0_mask & state.cr0.value());
				vmcb_cr0_shadow           = state.cr0.value();
#if 0
				vmcb->state_save_area.xcr0 = state.cr0();
#endif
			}

			if (state.cr2.charged())
				vmcb->state_save_area.cr2 = state.cr2.value();

			if (state.cr3.charged())
				vmcb->state_save_area.cr3 = state.cr3.value();

			if (state.cr4.charged()) {
				vmcb->state_save_area.cr4 = vmcb_cr4_set | (~vmcb_cr4_mask & state.cr4.value());
				vmcb_cr4_shadow           = state.cr4.value();
			}

			if (state.ctrl_primary.charged())
				vmcb->control_area.intercept_instruction0 = vmcb_ctrl0 |
				                                            state.ctrl_primary.value();

			if (state.ctrl_secondary.charged())
				vmcb->control_area.intercept_instruction1 = vmcb_ctrl1 |
				                                            state.ctrl_secondary.value();

			if (state.inj_info.charged()) {
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

			if (state.inj_error.charged()) {
				vmcb->control_area.eventinj &= ((1ULL << 32) - 1);
				uint64_t value = (0ULL + state.inj_error.value()) << 32;
				vmcb->control_area.eventinj |= value;
			}

			if (state.flags.charged())
				vmcb->state_save_area.rflags = state.flags.value();

			if (state.sp.charged())
				vmcb->state_save_area.rsp = state.sp.value();

			if (state.ip.charged())
				vmcb->state_save_area.rip = state.ip.value();

			if (state.efer.charged())
				vmcb->state_save_area.efer = state.efer.value() | AMD_SVM_ENABLE;

			if (state.intr_state.charged())
				vmcb->control_area.interrupt_shadow = state.intr_state.value();

			/* state.actv_state.charged() - not required for AMD */

			if (state.cs.charged()) {
				vmcb->state_save_area.cs.selector = state.cs.value().sel;
				vmcb->state_save_area.cs.attrib = state.cs.value().ar;
				vmcb->state_save_area.cs.limit = state.cs.value().limit;
				vmcb->state_save_area.cs.base = state.cs.value().base;
			}

			if (state.ss.charged()) {
				vmcb->state_save_area.ss.selector = state.ss.value().sel;
				vmcb->state_save_area.ss.attrib = state.ss.value().ar;
				vmcb->state_save_area.ss.limit = state.ss.value().limit;
				vmcb->state_save_area.ss.base = state.ss.value().base;
			}

			if (state.es.charged()) {
				vmcb->state_save_area.es.selector = state.es.value().sel;
				vmcb->state_save_area.es.attrib = state.es.value().ar;
				vmcb->state_save_area.es.limit = state.es.value().limit;
				vmcb->state_save_area.es.base = state.es.value().base;
			}

			if (state.ds.charged()) {
				vmcb->state_save_area.ds.selector = state.ds.value().sel;
				vmcb->state_save_area.ds.attrib = state.ds.value().ar;
				vmcb->state_save_area.ds.limit = state.ds.value().limit;
				vmcb->state_save_area.ds.base = state.ds.value().base;
			}

			if (state.fs.charged()) {
				vmcb->state_save_area.fs.selector = state.fs.value().sel;
				vmcb->state_save_area.fs.attrib = state.fs.value().ar;
				vmcb->state_save_area.fs.limit = state.fs.value().limit;
				vmcb->state_save_area.fs.base = state.fs.value().base;
			}

			if (state.gs.charged()) {
				vmcb->state_save_area.gs.selector = state.gs.value().sel;
				vmcb->state_save_area.gs.attrib = state.gs.value().ar;
				vmcb->state_save_area.gs.limit = state.gs.value().limit;
				vmcb->state_save_area.gs.base = state.gs.value().base;
			}

			if (state.tr.charged()) {
				vmcb->state_save_area.tr.selector = state.tr.value().sel;
				vmcb->state_save_area.tr.attrib = state.tr.value().ar;
				vmcb->state_save_area.tr.limit = state.tr.value().limit;
				vmcb->state_save_area.tr.base = state.tr.value().base;
			}

			if (state.ldtr.charged()) {
				vmcb->state_save_area.ldtr.selector = state.ldtr.value().sel;
				vmcb->state_save_area.ldtr.attrib = state.ldtr.value().ar;
				vmcb->state_save_area.ldtr.limit = state.ldtr.value().limit;
				vmcb->state_save_area.ldtr.base = state.ldtr.value().base;
			}

			if (state.idtr.charged()) {
				vmcb->state_save_area.idtr.base = state.idtr.value().base;
				vmcb->state_save_area.idtr.limit = state.idtr.value().limit;
			}

			if (state.gdtr.charged()) {
				vmcb->state_save_area.gdtr.base = state.gdtr.value().base;
				vmcb->state_save_area.gdtr.limit = state.gdtr.value().limit;
			}

			if (state.pdpte_0.charged() || state.pdpte_1.charged() ||
			    state.pdpte_2.charged() || state.pdpte_3.charged())
			{
				if (_show_error_unsupported_pdpte) {
					_show_error_unsupported_pdpte = false;
					error("PDPTE 0/1/2/3 not supported on Fiasco.OC");
				}
			}

			if (state.sysenter_cs.charged())
				vmcb->state_save_area.sysenter_cs = state.sysenter_cs.value();
			if (state.sysenter_sp.charged())
				vmcb->state_save_area.sysenter_esp = state.sysenter_sp.value();
			if (state.sysenter_ip.charged())
				vmcb->state_save_area.sysenter_eip = state.sysenter_ip.value();
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

		Foc_vcpu(Env &env, Vm_connection &vm, Vcpu_handler_base &handler,
		         enum Virt type)
		:
			Thread(env, "vcpu_thread", STACK_SIZE, _location(handler),
			       Weight(), env.cpu()),
			_vcpu_handler(handler),
			_exit_handler(handler.ep(), *this, &Foc_vcpu::_wrapper_dispatch),
			_vm_type(type)
		{
			Thread::start();

			_ep_handler = reinterpret_cast<Thread *>(&handler.rpc_ep());

			/* wait until thread is alive, e.g. Thread::cap() is valid */
			_startup.block();

			try {
				_rpc.construct(vm, this->cap(), *this);
			} catch (...) {
				terminate();
				join();
				throw;
			}
		}

		const Foc_vcpu& operator=(const Foc_vcpu &) = delete;
		Foc_vcpu(const Foc_vcpu&) = delete;

		void resume()
		{
			Mutex::Guard guard(_remote_mutex);

			if (_state_request == RUN || _state_request == PAUSE)
				return;

			_state_request = RUN;

			if (_state_current == NONE)
				_wake_up.up();
		}

		void terminate()
		{
			_state_request = TERMINATE;
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

				_state_request = PAUSE;

				/* Trigger pause exit */
				Foc::l4_cap_idx_t tid = native_thread().kcap;
				Foc::l4_cap_idx_t irq = tid + Foc::TASK_VCPU_IRQ_CAP;
				Foc::l4_irq_trigger(irq);

				if (_state_current == NONE)
					_wake_up.up();

				_remote_mutex.release();
				_state_ready.down();

				/*
				 * We're in the async dispatch, yet processing a non-pause exit.
				 * Signal that we have to wrap the dispatch loop around.
				 */
				if (_vcpu_state.exit_reason != VMEXIT_PAUSED)
					_extra_dispatch_up = true;
			} else {
				_state_ready.down();
			}

			if (cw.call_with_state(_vcpu_state)
			    || _extra_dispatch_up)
				resume();

			/*
			 * The regular exit was handled by the asynchronous dispatch handler
			 * triggered by the pause request.
			 *
			 * Fake finishing the exit dispatch so that the vCPU loop
			 * processes the asynchronously dispatched exit and provides
			 * the VMEXIT_PAUSED to the already pending dispatch function
			 * for the exit code.
			 */
			if (!_dispatching && _extra_dispatch_up)
				_exit_handler.ready_semaphore().up();
		}

		Foc_native_vcpu_rpc *rpc()   { return &*_rpc; }
};


static enum Virt virt_type(Env &env)
{
	try {
		Attached_rom_dataspace const info(env, "platform_info");
		Xml_node const features = info.xml().sub_node("hardware").sub_node("features");

		if (features.attribute_value("svm", false))
			return Virt::SVM;

		if (features.attribute_value("vmx", false))
			return Virt::VMX;
	} catch (...) { }

	return Virt::UNKNOWN;
}


/**************
 ** vCPU API **
 **************/

void Vm_connection::Vcpu::_with_state(Call_with_state &cw) { static_cast<Foc_native_vcpu_rpc &>(_native_vcpu).vcpu.with_state(cw); }


Vm_connection::Vcpu::Vcpu(Vm_connection &vm, Allocator &alloc,
                          Vcpu_handler_base &handler, Exit_config const &)
:
	_native_vcpu(*((new (alloc) Foc_vcpu(vm._env, vm, handler, virt_type(vm._env)))->rpc()))
{
	static_cast<Foc_native_vcpu_rpc &>(_native_vcpu).vcpu.resume();
}
