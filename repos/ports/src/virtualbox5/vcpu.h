/*
 * \brief  Genode VirtualBox SUPLib supplements
 * \author Alexander Boettcher
 * \author Norman Feske
 * \author Christian Helmuth
 */

/*
 * Copyright (C) 2013-2017 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#ifndef _VIRTUALBOX__VCPU_H_
#define _VIRTUALBOX__VCPU_H_

/* Genode includes */
#include <base/log.h>
#include <rom_session/connection.h>
#include <timer_session/connection.h>
#include <vm_session/connection.h>

#include <cpu/vm_state.h>

/* VirtualBox includes */
#include "PGMInternal.h" /* enable access to pgm.s.* */

#include "HMInternal.h" /* enable access to hm.s.* */
#include "CPUMInternal.h" /* enable access to cpum.s.* */

#include <VBox/vmm/vm.h>
#include <VBox/vmm/hm_svm.h>
#include <VBox/err.h>

#include <VBox/vmm/pdmapi.h>

#include <iprt/time.h>

/* Genode's VirtualBox includes */
#include "sup.h"

/* Genode libc pthread binding */
#include <internal/pthread.h>

#include <VBox/vmm/rem.h>

/*
 * VirtualBox stores segment attributes in Intel format using a 32-bit
 * value. Genode represents the attributes in packed format using a 16-bit
 * value.
 */
static inline Genode::uint16_t sel_ar_conv_to_genode(Genode::uint32_t v)
{
	return (v & 0xff) | ((v & 0x1f000) >> 4);
}


static inline Genode::uint32_t sel_ar_conv_from_genode(Genode::uint16_t v)
{
	return (v & 0xff) | (((uint32_t )v << 4) & 0x1f000);
}

class Vcpu_handler : public Genode::List<Vcpu_handler>::Element
{
	protected:

		Genode::Entrypoint             _ep;
		Genode::Lock                   _lock_emt;
		Genode::Semaphore              _sem_handler;
		Genode::Vm_state              *_state { nullptr };

		/* halt / wakeup handling with timeout support */
		Genode::Lock                   _r0_block_guard;
		Genode::Semaphore              _r0_block;
		Genode::uint64_t               _r0_wakeup_abs { 0 };

		/* information used for NPT/EPT handling */
		Genode::addr_t npt_ept_exit_addr { 0 };
		RTGCUINT       npt_ept_errorcode { 0 };
		bool           npt_ept_unmap     { false };

		/* state machine between EMT and EP thread of a vCPU */
		enum { RUNNING, PAUSED, IRQ_WIN, NPT_EPT } _vm_state { PAUSED };
		enum { PAUSE_EXIT, RUN } _next_state { RUN };

	private:

		bool               _irq_win = false;

		unsigned const     _cpu_id;
		PVM                _vm   { nullptr };
		PVMCPU             _vcpu { nullptr };

		unsigned int       _last_inj_info  = 0;
		unsigned int       _last_inj_error = 0;

		enum {
			REQ_IRQWIN_EXIT      = 0x1000U,
			IRQ_INJ_VALID_MASK   = 0x80000000UL,
			IRQ_INJ_NONE         = 0U,

			/*
			 * Intel® 64 and IA-32 Architectures Software Developer’s Manual 
			 * Volume 3C, Chapter 24.4.2.
			 * May 2012
			*/
			BLOCKING_BY_STI    = 1U << 0,
			BLOCKING_BY_MOV_SS = 1U << 1,
			ACTIVITY_STATE_ACTIVE = 0U,
			INTERRUPT_STATE_NONE  = 0U,
		};

	protected:

		int map_memory(Genode::Vm_connection &vm_session,
		               RTGCPHYS GCPhys, RTGCUINT vbox_fault_reason);

		Genode::addr_t     _vm_exits    = 0;
		Genode::addr_t     _recall_skip = 0;
		Genode::addr_t     _recall_req  = 0;
		Genode::addr_t     _recall_inv  = 0;
		Genode::addr_t     _recall_drop = 0;
		Genode::addr_t     _irq_request = 0;
		Genode::addr_t     _irq_inject  = 0;
		Genode::addr_t     _irq_drop    = 0;

		struct {
			unsigned intr_state;
			unsigned ctrl[2];
		} next_utcb;

		unsigned     _ept_fault_addr_type;

		Genode::uint64_t * pdpte_map(VM *pVM, RTGCPHYS cr3);

		void switch_to_hw(PCPUMCTX pCtx)
		{
			again:

			/* write FPU state */
			_state->fpu.value([&] (uint8_t *fpu, size_t const size) {
				if (size < sizeof(X86FXSTATE))
					Genode::error("fpu state too small");
				Genode::memcpy(fpu, pCtx->pXStateR3, sizeof(X86FXSTATE));
			});

			Assert(_vm_state == IRQ_WIN || _vm_state == PAUSED || _vm_state == NPT_EPT);
			Assert(_next_state == PAUSE_EXIT || _next_state == RUN);

			/* wake up vcpu ep handler */
			_sem_handler.up();

			/* wait for next exit */
			_lock_emt.lock();

			/* next time run - recall() may change this */
			_next_state = RUN;

			/* write FPU state of vCPU to pCtx */
			_state->fpu.value([&] (uint8_t *fpu, size_t const size) {
				if (size < sizeof(X86FXSTATE))
					Genode::error("fpu state too small");
				Genode::memcpy(pCtx->pXStateR3, fpu, sizeof(X86FXSTATE));
			});

			if (_vm_state == IRQ_WIN) {
				*_state = Genode::Vm_state {}; /* reset */
				_irq_window_pthread();
				goto again;
			} else
			if (_vm_state == NPT_EPT) {
				if (npt_ept_unmap) {
					Genode::error("NPT/EPT unmap not supported - stop");
					while (true) {
						_lock_emt.lock();
					}
				}

				Genode::addr_t const gp_map_addr = npt_ept_exit_addr & ~((1UL << 12) - 1);
				int res = attach_memory_to_vm(gp_map_addr, npt_ept_errorcode);
				if (res == VINF_SUCCESS) {
					*_state = Genode::Vm_state {}; /* reset */
					goto again;
				}
			}

			if (!(_vm_state == PAUSED || _vm_state == NPT_EPT))
				Genode::error("which state we are ? ", (int)_vm_state, " ", Genode::Thread::myself()->name());

			Assert(_vm_state == PAUSED || _vm_state == NPT_EPT);
		}

		void _default_handler()
		{
			if (_vm_state != RUNNING)
				Genode::error(__func__, " _vm_state=", (int)_vm_state, " exit_reason=", Genode::Hex(_state->exit_reason));
			Assert(_vm_state == RUNNING);

			Assert(_state->actv_state.value() == ACTIVITY_STATE_ACTIVE);
			Assert(!(_state->inj_info.value() & IRQ_INJ_VALID_MASK));

			_vm_exits ++;

			_vm_state = PAUSED;

			_lock_emt.unlock();
		}

		bool _recall_handler()
		{
			if (_vm_state != RUNNING)
				Genode::error(__func__, " _vm_state=", (int)_vm_state, " exit_reason=", Genode::Hex(_state->exit_reason));
			Assert(_vm_state == RUNNING);

			_vm_exits ++;
			_recall_inv ++;

			Assert(_state->actv_state.value() == ACTIVITY_STATE_ACTIVE);

			if (_state->inj_info.value() & IRQ_INJ_VALID_MASK) {

				Assert(_state->flags.value() & X86_EFL_IF);

				if (_state->intr_state.value() != INTERRUPT_STATE_NONE)
					Genode::log("intr state ", Genode::Hex(_state->intr_state.value()),
					            " ", Genode::Hex(_state->intr_state.value() & 0xf));

				Assert(_state->intr_state.value() == INTERRUPT_STATE_NONE);

				if (!continue_hw_accelerated())
					_recall_drop ++;

				/* got recall during irq injection and the guest is ready for
				 * delivery of IRQ - just continue */
				return /* no-wait */ false;
			}

			/* are we forced to go back to emulation mode ? */
			if (!continue_hw_accelerated()) {
				/* go back to emulation mode */
				_default_handler();
				return /* wait */ true;
			}

			/* check whether we have to request irq injection window */
			if (check_to_request_irq_window(_vcpu)) {
				*_state = Genode::Vm_state {}; /* reset */
				_state->inj_info.value(_state->inj_info.value());
				_irq_win = true;
				return /* no-wait */ false;
			}

			_default_handler();
			return /* wait */ true;
		}

		inline bool vbox_to_state(VM *pVM, PVMCPU pVCpu)
		{
			typedef Genode::Vm_state::Range Range;

			PCPUMCTX pCtx  = CPUMQueryGuestCtxPtr(pVCpu);

			_state->ip.value(pCtx->rip);
			_state->sp.value(pCtx->rsp);

			_state->ax.value(pCtx->rax);
			_state->bx.value(pCtx->rbx);
			_state->cx.value(pCtx->rcx);
			_state->dx.value(pCtx->rdx);

			_state->bp.value(pCtx->rbp);
			_state->si.value(pCtx->rsi);
			_state->di.value(pCtx->rdi);

			_state->r8.value(pCtx->r8);
			_state->r9.value(pCtx->r9);
			_state->r10.value(pCtx->r10);
			_state->r11.value(pCtx->r11);
			_state->r12.value(pCtx->r12);
			_state->r13.value(pCtx->r13);
			_state->r14.value(pCtx->r14);
			_state->r15.value(pCtx->r15);

			_state->flags.value(pCtx->rflags.u);

			_state->sysenter_cs.value(pCtx->SysEnter.cs);
			_state->sysenter_sp.value(pCtx->SysEnter.esp);
			_state->sysenter_ip.value(pCtx->SysEnter.eip);

			_state->dr7.value(pCtx->dr[7]);

			_state->cr0.value(pCtx->cr0);
			_state->cr2.value(pCtx->cr2);
			_state->cr3.value(pCtx->cr3);
			_state->cr4.value(pCtx->cr4);

			_state->idtr.value(Range{pCtx->idtr.pIdt, pCtx->idtr.cbIdt});
			_state->gdtr.value(Range{pCtx->gdtr.pGdt, pCtx->gdtr.cbGdt});

			_state->efer.value(CPUMGetGuestEFER(pVCpu));

			/*
			 * Update the PDPTE registers if necessary
			 *
			 * Intel manual sections 4.4.1 of Vol. 3A and 26.3.2.4 of Vol. 3C
			 * indicate the conditions when this is the case. The following
			 * code currently does not check if the recompiler modified any
			 * CR registers, which means the update can happen more often
			 * than really necessary.
			 */
			if (pVM->hm.s.vmx.fSupported &&
				CPUMIsGuestPagingEnabledEx(pCtx) &&
				CPUMIsGuestInPAEModeEx(pCtx)) {

				Genode::uint64_t *pdpte = pdpte_map(pVM, pCtx->cr3);

				_state->pdpte_0.value(pdpte[0]);
				_state->pdpte_1.value(pdpte[1]);
				_state->pdpte_2.value(pdpte[2]);
				_state->pdpte_3.value(pdpte[3]);
			}

			_state->star.value(pCtx->msrSTAR);
			_state->lstar.value(pCtx->msrLSTAR);
			_state->fmask.value(pCtx->msrSFMASK);
			_state->kernel_gs_base.value(pCtx->msrKERNELGSBASE);

			/* from HMVMXR0.cpp */
			bool interrupt_pending    = false;
			uint8_t tpr               = 0;
			uint8_t pending_interrupt = 0;
			PDMApicGetTPR(pVCpu, &tpr, &interrupt_pending, &pending_interrupt);

			_state->tpr.value(tpr);
			_state->tpr_threshold.value(0);

			if (interrupt_pending) {
				const uint8_t pending_priority = (pending_interrupt >> 4) & 0xf;
				const uint8_t tpr_priority = (tpr >> 4) & 0xf;
				if (pending_priority <= tpr_priority)
					_state->tpr_threshold.value(pending_priority);
				else
					_state->tpr_threshold.value(tpr_priority);
			}

			return true;
		}


		inline bool state_to_vbox(VM *pVM, PVMCPU pVCpu)
		{
			PCPUMCTX pCtx  = CPUMQueryGuestCtxPtr(pVCpu);

			pCtx->rip = _state->ip.value();
			pCtx->rsp = _state->sp.value();

			pCtx->rax = _state->ax.value();
			pCtx->rbx = _state->bx.value();
			pCtx->rcx = _state->cx.value();
			pCtx->rdx = _state->dx.value();

			pCtx->rbp = _state->bp.value();
			pCtx->rsi = _state->si.value();
			pCtx->rdi = _state->di.value();
			pCtx->rflags.u = _state->flags.value();

			pCtx->r8  = _state->r8.value();
			pCtx->r9  = _state->r9.value();
			pCtx->r10 = _state->r10.value();
			pCtx->r11 = _state->r11.value();
			pCtx->r12 = _state->r12.value();
			pCtx->r13 = _state->r13.value();
			pCtx->r14 = _state->r14.value();
			pCtx->r15 = _state->r15.value();

			pCtx->dr[7] = _state->dr7.value();

			if (pCtx->SysEnter.cs != _state->sysenter_cs.value())
				CPUMSetGuestMsr(pVCpu, MSR_IA32_SYSENTER_CS, _state->sysenter_cs.value());

			if (pCtx->SysEnter.esp != _state->sysenter_sp.value())
				CPUMSetGuestMsr(pVCpu, MSR_IA32_SYSENTER_ESP, _state->sysenter_sp.value());

			if (pCtx->SysEnter.eip != _state->sysenter_ip.value())
				CPUMSetGuestMsr(pVCpu, MSR_IA32_SYSENTER_EIP, _state->sysenter_ip.value());

			if (pCtx->idtr.cbIdt != _state->idtr.value().limit ||
			    pCtx->idtr.pIdt  != _state->idtr.value().base)
				CPUMSetGuestIDTR(pVCpu, _state->idtr.value().base, _state->idtr.value().limit);

			if (pCtx->gdtr.cbGdt != _state->gdtr.value().limit ||
			    pCtx->gdtr.pGdt  != _state->gdtr.value().base)
				CPUMSetGuestGDTR(pVCpu, _state->gdtr.value().base, _state->gdtr.value().limit);

			CPUMSetGuestEFER(pVCpu, _state->efer.value());

			if (pCtx->cr0 != _state->cr0.value())
				CPUMSetGuestCR0(pVCpu, _state->cr0.value());

			if (pCtx->cr2 != _state->cr2.value())
				CPUMSetGuestCR2(pVCpu, _state->cr2.value());

			if (pCtx->cr3 != _state->cr3.value()) {
				CPUMSetGuestCR3(pVCpu, _state->cr3.value());
				VMCPU_FF_SET(pVCpu, VMCPU_FF_HM_UPDATE_CR3);
			}

			if (pCtx->cr4 != _state->cr4.value())
				CPUMSetGuestCR4(pVCpu, _state->cr4.value());

			if (pCtx->msrSTAR != _state->star.value())
				CPUMSetGuestMsr(pVCpu, MSR_K6_STAR, _state->star.value());

			if (pCtx->msrLSTAR != _state->lstar.value())
				CPUMSetGuestMsr(pVCpu, MSR_K8_LSTAR, _state->lstar.value());

			if (pCtx->msrSFMASK != _state->fmask.value())
				CPUMSetGuestMsr(pVCpu, MSR_K8_SF_MASK, _state->fmask.value());

			if (pCtx->msrKERNELGSBASE != _state->kernel_gs_base.value())
				CPUMSetGuestMsr(pVCpu, MSR_K8_KERNEL_GS_BASE, _state->kernel_gs_base.value());

			const uint32_t tpr = _state->tpr.value();

			/* reset message transfer descriptor for next invocation */
			Assert (!(_state->inj_info.value() & IRQ_INJ_VALID_MASK));
			next_utcb.intr_state = _state->intr_state.value();
			next_utcb.ctrl[0]    = _state->ctrl_primary.value();
			next_utcb.ctrl[1]    = _state->ctrl_secondary.value();

			if (next_utcb.intr_state & 3) {
				next_utcb.intr_state &= ~3U;
			}

			VMCPU_FF_CLEAR(pVCpu, VMCPU_FF_TO_R3);

			CPUMSetChangedFlags(pVCpu, CPUM_CHANGED_FPU_REM);
			pVCpu->cpum.s.fUseFlags |=  (CPUM_USED_FPU_GUEST | CPUM_USED_FPU_SINCE_REM);
			
			if (_state->intr_state.value() != 0) {
				Assert(_state->intr_state.value() == BLOCKING_BY_STI ||
				       _state->intr_state.value() == BLOCKING_BY_MOV_SS);
				EMSetInhibitInterruptsPC(pVCpu, pCtx->rip);
			} else
				VMCPU_FF_CLEAR(pVCpu, VMCPU_FF_INHIBIT_INTERRUPTS);

			PDMApicSetTPR(pVCpu, tpr);

			return true;
		}


		inline bool check_to_request_irq_window(PVMCPU pVCpu)
		{
			if (VMCPU_FF_IS_SET(pVCpu, VMCPU_FF_INHIBIT_INTERRUPTS))
				return false;

			if (!TRPMHasTrap(pVCpu) &&
				!VMCPU_FF_IS_PENDING(pVCpu, (VMCPU_FF_INTERRUPT_APIC |
				                             VMCPU_FF_INTERRUPT_PIC)))
				return false;

			_irq_request++;

			unsigned const vector = 0;
			_state->inj_info.value(REQ_IRQWIN_EXIT | vector);

			return true;
		}


		void _irq_window()
		{
			if (_vm_state != RUNNING)
				Genode::error(__func__, " _vm_state=", (int)_vm_state, " exit_reason=", Genode::Hex(_state->exit_reason));
			Assert(_vm_state == RUNNING);

			_vm_exits ++;

			_vm_state = IRQ_WIN;
			_lock_emt.unlock();
		}

		void _npt_ept()
		{
			if (_vm_state != RUNNING)
				Genode::error(__func__, " _vm_state=", (int)_vm_state, " exit_reason=", Genode::Hex(_state->exit_reason));
			Assert(_vm_state == RUNNING);

			_vm_exits ++;

			_vm_state = NPT_EPT;
			_lock_emt.unlock();
		}

		void _irq_window_pthread()
		{
			PVMCPU   pVCpu = _vcpu;

			Assert(_state->intr_state.value() == INTERRUPT_STATE_NONE);
			Assert(_state->flags.value() & X86_EFL_IF);
			Assert(!VMCPU_FF_IS_SET(pVCpu, VMCPU_FF_INHIBIT_INTERRUPTS));
			Assert(!(_state->inj_info.value() & IRQ_INJ_VALID_MASK));

			Assert(_irq_win);

			_irq_win = false;

			/* request current tpr state from guest, it may block IRQs */
			PDMApicSetTPR(pVCpu, _state->tpr_threshold.value());

			if (!TRPMHasTrap(pVCpu)) {

				bool res = VMCPU_FF_TEST_AND_CLEAR(pVCpu, VMCPU_FF_INTERRUPT_NMI);
				if (res)
					Genode::log("NMI was set");

				if (VMCPU_FF_IS_PENDING(pVCpu, (VMCPU_FF_INTERRUPT_APIC |
				                               VMCPU_FF_INTERRUPT_PIC))) {

					uint8_t irq;
					int rc = PDMGetInterrupt(pVCpu, &irq);
					Assert(RT_SUCCESS(rc));

					rc = TRPMAssertTrap(pVCpu, irq, TRPM_HARDWARE_INT);
					Assert(RT_SUCCESS(rc));
				}

				if (!TRPMHasTrap(pVCpu)) {
					_irq_drop++;
					/* happens if PDMApicSetTPR (see above) mask IRQ */
					_state->inj_info.value(IRQ_INJ_NONE);
					Genode::error("virq window pthread aaaaaaa while loop");
					return;
				}
			}
			_irq_inject++;

			/*
			 * If we have no IRQ for injection, something with requesting the
			 * IRQ window went wrong. Probably it was forgotten to be reset.
			 */
			Assert(TRPMHasTrap(pVCpu));

		   	/* interrupt can be dispatched */
			uint8_t     u8Vector;
			TRPMEVENT   enmType;
			SVMEVENT    Event;
			RTGCUINT    u32ErrorCode;
			RTGCUINTPTR GCPtrFaultAddress;
			uint8_t     cbInstr;

			Event.u = 0;

			/* If a new event is pending, then dispatch it now. */
			int rc = TRPMQueryTrapAll(pVCpu, &u8Vector, &enmType, &u32ErrorCode, 0, 0);
			AssertRC(rc);
			Assert(enmType == TRPM_HARDWARE_INT);
			Assert(u8Vector != X86_XCPT_NMI);

			/* Clear the pending trap. */
			rc = TRPMResetTrap(pVCpu);
			AssertRC(rc);

			Event.n.u8Vector = u8Vector;
			Event.n.u1Valid  = 1;
			Event.n.u32ErrorCode = u32ErrorCode;

			Event.n.u3Type = SVM_EVENT_EXTERNAL_IRQ;

			_state->inj_info.value(Event.u);
			_state->inj_error.value(Event.n.u32ErrorCode);

			_last_inj_info = _state->inj_info.value();
			_last_inj_error = _state->inj_error.value();

/*
			Genode::log("type:info:vector ", Genode::Hex(Event.n.u3Type),
			         Genode::Hex(utcb->inj_info), Genode::Hex(u8Vector),
			         " intr:actv - ", Genode::Hex(utcb->intr_state),
			         Genode::Hex(utcb->actv_state), " mtd ",
			         Genode::Hex(utcb->mtd));
*/
		}


		inline bool continue_hw_accelerated(bool verbose = false)
		{
			uint32_t check_vm = VM_FF_HM_TO_R3_MASK | VM_FF_REQUEST
			                    | VM_FF_PGM_POOL_FLUSH_PENDING
			                    | VM_FF_PDM_DMA;
			uint32_t check_vcpu = VMCPU_FF_HM_TO_R3_MASK
			                      | VMCPU_FF_PGM_SYNC_CR3
			                      | VMCPU_FF_PGM_SYNC_CR3_NON_GLOBAL
			                      | VMCPU_FF_REQUEST;

			if (!VM_FF_IS_PENDING(_vm, check_vm) &&
			    !VMCPU_FF_IS_PENDING(_vcpu, check_vcpu))
				return true;

			Assert(!(VM_FF_IS_PENDING(_vm, VM_FF_PGM_NO_MEMORY)));

#define VERBOSE_VM(flag) \
			do { \
				if (VM_FF_IS_PENDING(_vm, flag)) \
					Genode::log("flag ", flag, " pending"); \
			} while (0)

#define VERBOSE_VMCPU(flag) \
			do { \
				if (VMCPU_FF_IS_PENDING(_vcpu, flag)) \
					Genode::log("flag ", flag, " pending"); \
			} while (0)

			if (verbose) {
				/*
				 * VM_FF_HM_TO_R3_MASK
				 */
				VERBOSE_VM(VM_FF_TM_VIRTUAL_SYNC);
				VERBOSE_VM(VM_FF_PGM_NEED_HANDY_PAGES);
				/* handled by the assertion above */
				/* VERBOSE_VM(VM_FF_PGM_NO_MEMORY); */
				VERBOSE_VM(VM_FF_PDM_QUEUES);
				VERBOSE_VM(VM_FF_EMT_RENDEZVOUS);

				VERBOSE_VM(VM_FF_REQUEST);
				VERBOSE_VM(VM_FF_PGM_POOL_FLUSH_PENDING);
				VERBOSE_VM(VM_FF_PDM_DMA);

				/*
				 * VMCPU_FF_HM_TO_R3_MASK
				 */
				VERBOSE_VMCPU(VMCPU_FF_TO_R3);
				/* when this flag gets set, a recall request follows */
				/* VERBOSE_VMCPU(VMCPU_FF_TIMER); */
				VERBOSE_VMCPU(VMCPU_FF_PDM_CRITSECT);

				VERBOSE_VMCPU(VMCPU_FF_PGM_SYNC_CR3);
				VERBOSE_VMCPU(VMCPU_FF_PGM_SYNC_CR3_NON_GLOBAL);
				VERBOSE_VMCPU(VMCPU_FF_REQUEST);
			}

#undef VERBOSE_VMCPU
#undef VERBOSE_VM

			return false;
		}

		virtual bool hw_load_state(Genode::Vm_state *, VM *, PVMCPU) = 0;
		virtual bool hw_save_state(Genode::Vm_state *, VM *, PVMCPU) = 0;
		virtual int vm_exit_requires_instruction_emulation(PCPUMCTX) = 0;

		virtual void pause_vm() = 0;
		virtual int attach_memory_to_vm(RTGCPHYS const,
		                                RTGCUINT vbox_fault_reason) = 0;

	public:

		enum Exit_condition
		{
			SVM_NPT     = 0xfc,
			SVM_INVALID = 0xfd,

			VCPU_STARTUP  = 0xfe,

			RECALL        = 0xff,
		};


		Vcpu_handler(Genode::Env &env, size_t stack_size,
		             Genode::Affinity::Location location,
		             unsigned int cpu_id)
		:
			_ep(env, stack_size,
			    Genode::String<12>("EP-EMT-", cpu_id).string(), location),
			_cpu_id(cpu_id)
		{ }

		unsigned int cpu_id() { return _cpu_id; }


		void recall(PVM vm)
		{
			if (!_vm || !_vcpu) {
				_vm   = vm;
				_vcpu = &vm->aCpus[_cpu_id];
			}

			if (_vm != vm || _vcpu != &vm->aCpus[_cpu_id])
				Genode::error("wrong CPU !?");

			_recall_req ++;

			if (_irq_win) {
				_recall_skip ++;
				return;
			}

			asm volatile ("":::"memory");

			if (_vm_state != PAUSED)
				pause_vm();

			_next_state = PAUSE_EXIT;

#if 0
			if (_recall_req % 1000 == 0) {
				using Genode::log;

				while (other) {
					log(other->_cpu_id, " exits=", other->_vm_exits,
					    " req:skip:drop,inv recall=", other->_recall_req, ":",
					    other->_recall_skip, ":", other->_recall_drop, ":",
					    other->_recall_inv, " req:inj:drop irq=",
					    other->_irq_request, ":", other->_irq_inject, ":",
					    other->_irq_drop);

					other = other->next();
				}
			}
#endif
		}

		void check_time()
		{
			{
				Genode::Lock_guard<Genode::Lock> lock(_r0_block_guard);

				const uint64_t u64NowGip = RTTimeNanoTS();
				if (!_r0_wakeup_abs || _r0_wakeup_abs >= u64NowGip)
					return;
			}

			wake_up();
		}

		void halt(Genode::uint64_t rttime_abs)
		{
			{
				Genode::Lock_guard<Genode::Lock> lock(_r0_block_guard);
				_r0_wakeup_abs = rttime_abs;
			}

			_r0_block.down();
		}

		void wake_up()
		{
			{
				Genode::Lock_guard<Genode::Lock> lock(_r0_block_guard);
				_r0_wakeup_abs = 0;
			}

			_r0_block.up();
		}

		int run_hw(PVMR0 pVMR0)
		{
			VM     * pVM   = reinterpret_cast<VM *>(pVMR0);
			PVMCPU   pVCpu = &pVM->aCpus[_cpu_id];
			PCPUMCTX pCtx  = CPUMQueryGuestCtxPtr(pVCpu);

			if (!_vm || !_vcpu) {
				_vm   = pVM;
				_vcpu = &pVM->aCpus[_cpu_id];
			}

			if (_vm != pVM || _vcpu != &pVM->aCpus[_cpu_id])
				Genode::error("wrong CPU !?");

			/* take the utcb state prepared during the last exit */
			_state->inj_info.value(IRQ_INJ_NONE);
			_state->intr_state.value(next_utcb.intr_state);
			_state->actv_state.value(ACTIVITY_STATE_ACTIVE);
			_state->ctrl_primary.value(next_utcb.ctrl[0]);
			_state->ctrl_secondary.value(next_utcb.ctrl[1]);

			/* Transfer vCPU state from vbox to Genode format */
			if (!vbox_to_state(pVM, pVCpu) ||
				!hw_load_state(_state, pVM, pVCpu)) {

				Genode::error("loading vCPU state failed");
				return VERR_INTERNAL_ERROR;
			}

			/* check whether to request interrupt window for injection */
			_irq_win = check_to_request_irq_window(pVCpu);

			/*
			 * Flag vCPU to be "pokeable" by external events such as interrupts
			 * from virtual devices. Only if this flag is set, the
			 * 'vmR3HaltGlobal1NotifyCpuFF' function calls 'SUPR3CallVMMR0Ex'
			 * with VMMR0_DO_GVMM_SCHED_POKE as argument to indicate such
			 * events. This function, in turn, will recall the vCPU.
			 */
			VMCPU_SET_STATE(pVCpu, VMCPUSTATE_STARTED_EXEC);

			/* switch to hardware accelerated mode */
			switch_to_hw(pCtx);

			Assert(_state->actv_state.value() == ACTIVITY_STATE_ACTIVE);

			/* see hmR0VmxExitToRing3 - sync recompiler state */
			CPUMSetChangedFlags(pVCpu, CPUM_CHANGED_SYSENTER_MSR |
			                    CPUM_CHANGED_LDTR | CPUM_CHANGED_GDTR |
			                    CPUM_CHANGED_IDTR | CPUM_CHANGED_TR |
			                    CPUM_CHANGED_HIDDEN_SEL_REGS |
			                    CPUM_CHANGED_GLOBAL_TLB_FLUSH);

			VMCPU_SET_STATE(pVCpu, VMCPUSTATE_STARTED);

			/* Transfer vCPU state from Genode to vbox format */
			if (!state_to_vbox(pVM, pVCpu) ||
				!hw_save_state(_state, pVM, pVCpu)) {

				Genode::error("saving vCPU state failed");
				return VERR_INTERNAL_ERROR;
			}

#ifdef VBOX_WITH_REM
			REMFlushTBs(pVM);
#endif

			/* track guest mode changes - see VMM/VMMAll/IEMAllCImpl.cpp.h */
			PGMChangeMode(pVCpu, pCtx->cr0, pCtx->cr4, pCtx->msrEFER);

			int rc = vm_exit_requires_instruction_emulation(pCtx);

			/* evaluated in VMM/include/EMHandleRCTmpl.h */
			return rc;
		}
};

#endif /* _VIRTUALBOX__VCPU_H_ */
