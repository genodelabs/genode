/*
 * \brief  SUPLib vCPU utility
 * \author Alexander Boettcher
 * \author Norman Feske
 * \author Christian Helmuth
 */

/*
 * Copyright (C) 2013-2021 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

/* Genode includes */
#include <cpu/vcpu_state.h>

/* VirtualBox includes */
#include <VBox/vmm/cpum.h> /* must be included before CPUMInternal.h */
#include <CPUMInternal.h>  /* enable access to cpum.s.* */
#include <HMInternal.h>    /* enable access to hm.s.* */
#include <VBox/vmm/vm.h>
#include <VBox/vmm/hm.h>
#include <VBox/vmm/apic.h>
#include <VBox/vmm/em.h>
#include <VBox/vmm/pdmapi.h>
#include <VBox/err.h>
#include <iprt/time.h>

/* libc includes */
#include <stdlib.h> /* for exit() */
#include <pthread.h>

/* local includes */
#include <vcpu.h>
#include <pthread_emt.h>


/*
 * VirtualBox stores segment attributes in Intel format using 17 bits of a
 * 32-bit value, which includes bits 19:16 of segment limit (see
 * X86DESCATTRBITS).
 *
 * Genode represents the attributes in packed SVM VMCB format using 13 bits of
 * a 16-bit value without segment-limit bits.
 */
static inline Genode::uint16_t sel_ar_conv_to_genode(Genode::uint32_t v)
{
	return (v & 0xff) | ((v & 0x1f000) >> 4);
}


static inline Genode::uint32_t sel_ar_conv_from_genode(Genode::uint16_t v)
{
	return (v & 0xff) | (((uint32_t )v << 4) & 0x1f000);
}

/* XXX these headers use the functions defined above */
#include <svm.h>
#include <vmx.h>


/*****************
 ** SVM handler **
 *****************/

void Sup::Vcpu_handler_svm::_svm_default() { _default_handler(); }
void Sup::Vcpu_handler_svm::_svm_vintr()   { _irq_window(); }


void Sup::Vcpu_handler_svm::_svm_ioio()
{
	Vcpu_state &state { _vcpu.state() };

	if (state.qual_primary.value() & 0x4) {
		unsigned ctrl0 = state.ctrl_primary.value();

		Genode::warning("invalid gueststate");

		state.discharge();

		state.ctrl_primary.charge(ctrl0);
		state.ctrl_secondary.charge(0);

		_run_vm();
	} else
		_default_handler();
}


template <unsigned X> void Sup::Vcpu_handler_svm::_svm_npt()
{
	Vcpu_state &state { _vcpu.state() };

	bool           const unmap          = state.qual_primary.value() & 1;
	Genode::addr_t const exit_addr      = state.qual_secondary.value();
	RTGCUINT       const vbox_errorcode = state.qual_primary.value();

	_npt_ept_exit_addr = exit_addr;
	_npt_ept_unmap     = unmap;
	_npt_ept_errorcode = vbox_errorcode;

	_npt_ept();
}


void Sup::Vcpu_handler_svm::_svm_startup()
{
	/* enable VM exits for CPUID */
	_next_utcb.ctrl[0] = SVM_CTRL_INTERCEPT_CPUID;
	_next_utcb.ctrl[1] = 0;
}


void Sup::Vcpu_handler_svm::_handle_exit()
{
	/*
	 * Table B-1. 070h 63:0 EXITCODE
	 *
	 * Appendix C SVM Intercept Exit Codes defines only
	 * 0x000..0x403 plus -1 and -2
	 */
	unsigned short const exit = _vcpu.state().exit_reason & 0xffff;
	bool recall_wait = true;

//	Genode::warning(__PRETTY_FUNCTION__, ": ", HMGetSvmExitName(exit));

	switch (exit) {
	case SVM_EXIT_IOIO:  _svm_ioio(); break;
	case SVM_EXIT_VINTR: _svm_vintr(); break;
//	case SVM_EXIT_RDTSC: _svm_default(); break;
	case SVM_EXIT_MSR:
		/* XXX distiguish write from read */
		_last_exit_triggered_by_wrmsr = true;
		_svm_default();
		break;
	case SVM_NPT:        _svm_npt<SVM_NPT>(); break;
	case SVM_EXIT_HLT:   _svm_default(); break;
	case SVM_EXIT_CPUID: _svm_default(); break;
	case RECALL:
		recall_wait = Vcpu_handler::_recall_handler();
		break;
	case VCPU_STARTUP:
		_svm_startup();
		/* pause - no resume */
		break;
	default:
		Genode::error(__func__, " unknown exit - stop - ",
		              Genode::Hex(exit));
		_vm_state = PAUSED;
		return;
	}

	if (exit == RECALL && !recall_wait) {
		_vm_state = RUNNING;
		_run_vm();
		return;
	}

	/* wait until EMT thread wake's us up */
	/* TODO XXX _sem_handler.down(); */

	/* resume vCPU */
	_vm_state = RUNNING;
	if (_next_state == RUN)
		_run_vm();
	else
		_pause_vm(); /* cause pause exit */
}


bool Sup::Vcpu_handler_svm::_hw_save_state(VM * pVM, PVMCPU pVCpu)
{
	return svm_save_state(_vcpu.state(), pVM, pVCpu);
}


bool Sup::Vcpu_handler_svm::_hw_load_state(VM * pVM, PVMCPU pVCpu)
{
	return svm_load_state(_vcpu.state(), pVM, pVCpu);
}


int Sup::Vcpu_handler_svm::_vm_exit_requires_instruction_emulation(PCPUMCTX)
{
	if (_state->exit_reason == RECALL)
		return VINF_SUCCESS;

	return VINF_EM_RAW_EMULATE_INSTR;
}


Sup::Vcpu_handler_svm::Vcpu_handler_svm(Genode::Env &env,
                                        unsigned int cpu_id,
                                        Pthread::Emt &emt,
                                        Genode::Vm_connection &vm_connection,
                                        Genode::Allocator &alloc)
:
	Vcpu_handler(env, cpu_id, emt),
	_handler(_emt.genode_ep(), *this, &Vcpu_handler_svm::_handle_exit),
	_vm_connection(vm_connection),
	_vcpu(_vm_connection, alloc, _handler, _exit_config)
{
	_state = &_vcpu.state();

	/* run vCPU until initial startup exception */
	_vcpu.run();
	_emt.switch_to_vcpu();
}


/*****************
 ** VMX handler **
 *****************/

void Sup::Vcpu_handler_vmx::_vmx_default() { _default_handler(); }
void Sup::Vcpu_handler_vmx::_vmx_irqwin()  { _irq_window(); }
void Sup::Vcpu_handler_vmx::_vmx_mov_crx() { _default_handler(); }


template <unsigned X> void Sup::Vcpu_handler_vmx::_vmx_ept()
{
	Genode::addr_t const exit_qual = _state->qual_primary.value();
	Genode::addr_t const exit_addr = _state->qual_secondary.value();
	bool           const unmap     = exit_qual & 0x38;

	RTGCUINT vbox_errorcode = 0;
	if (exit_qual & VMX_EXIT_QUAL_EPT_INSTR_FETCH)
		vbox_errorcode |= X86_TRAP_PF_ID;
	if (exit_qual & VMX_EXIT_QUAL_EPT_DATA_WRITE)
		vbox_errorcode |= X86_TRAP_PF_RW;
	if (exit_qual & VMX_EXIT_QUAL_EPT_ENTRY_PRESENT)
		vbox_errorcode |= X86_TRAP_PF_P;

	_npt_ept_exit_addr = exit_addr;
	_npt_ept_unmap     = unmap;
	_npt_ept_errorcode = vbox_errorcode;

	_npt_ept();
}



void Sup::Vcpu_handler_vmx::_vmx_startup()
{
	/* configure VM exits to get */
	/* from src/VBox/VMM/VMMR0/HWVMXR0.cpp of virtualbox sources  */
	_next_utcb.ctrl[0] = 0
	                   | VMX_PROC_CTLS_HLT_EXIT
	                   | VMX_PROC_CTLS_MOV_DR_EXIT
	                   | VMX_PROC_CTLS_UNCOND_IO_EXIT
//	                   | VMX_PROC_CTLS_MONITOR_EXIT
//	                   | VMX_PROC_CTLS_MWAIT_EXIT
//	                   | VMX_PROC_CTLS_CR8_LOAD_EXIT
//	                   | VMX_PROC_CTLS_CR8_STORE_EXIT
	                   | VMX_PROC_CTLS_USE_TPR_SHADOW
	                   | VMX_PROC_CTLS_RDPMC_EXIT
//	                   | VMX_PROC_CTLS_PAUSE_EXIT
	/*
	 * Disable trapping RDTSC for now as it creates a huge load with
	 * VM guests that execute it frequently.
	 */
//	                  | VMX_VMCS_CTRL_PROC_EXEC_RDTSC_EXIT
	                  ;

	_next_utcb.ctrl[1] = 0
	                   | VMX_PROC_CTLS2_APIC_REG_VIRT
	                   | VMX_PROC_CTLS2_WBINVD_EXIT
	                   | VMX_PROC_CTLS2_UNRESTRICTED_GUEST
	                   | VMX_PROC_CTLS2_VPID
	                   | VMX_PROC_CTLS2_RDTSCP
	                   | VMX_PROC_CTLS2_EPT
	                   | VMX_PROC_CTLS2_INVPCID
	                   ;
}


void Sup::Vcpu_handler_vmx::_vmx_triple()
{
	Genode::error("triple fault - dead");
	exit(-1);
}


__attribute__((noreturn)) void Sup::Vcpu_handler_vmx::_vmx_invalid()
{
	unsigned const dubious = _state->inj_info.value() |
	                         _state->intr_state.value() |
	                         _state->actv_state.value();
	if (dubious)
		Genode::warning(__func__, " - dubious -"
		                " inj_info=", Genode::Hex(_state->inj_info.value()),
		                " inj_error=", Genode::Hex(_state->inj_error.value()),
		                " intr_state=", Genode::Hex(_state->intr_state.value()),
		                " actv_state=", Genode::Hex(_state->actv_state.value()));

	Genode::error("invalid guest state - dead");

	/* FIXME exit() cannot be called in VCPU mode */
	exit(-1);
}


void Sup::Vcpu_handler_vmx::_handle_exit()
{
	/* table 24-14. Format of Exit Reason - 15:0 Basic exit reason */
	unsigned short const exit = _state->exit_reason & 0xffff;
	bool recall_wait = true;

//	Genode::warning(__PRETTY_FUNCTION__, ": ", HMGetVmxExitName(exit));

	switch (exit) {
	case VMX_EXIT_TRIPLE_FAULT: _vmx_triple(); break;
	case VMX_EXIT_INIT_SIGNAL: _vmx_default(); break;
	case VMX_EXIT_INT_WINDOW: _vmx_irqwin(); break;
	case VMX_EXIT_TASK_SWITCH: _vmx_default(); break;
	case VMX_EXIT_CPUID: _vmx_default(); break;
	case VMX_EXIT_HLT: _vmx_default(); break;
	/* we don't support tsc offsetting for now - so let the rdtsc exit */
	case VMX_EXIT_RDTSC: _vmx_default(); break;
	case VMX_EXIT_RDTSCP: _vmx_default(); break;
	case VMX_EXIT_VMCALL: _vmx_default(); break;
	case VMX_EXIT_IO_INSTR: _vmx_default(); break;
	case VMX_EXIT_RDMSR: _vmx_default(); break;
	case VMX_EXIT_WRMSR:
		_last_exit_triggered_by_wrmsr = true;
		_vmx_default();
		break;
	case VMX_EXIT_ERR_INVALID_GUEST_STATE: _vmx_invalid(); break;
	case VMX_EXIT_PAUSE: _vmx_default(); break;
	case VMX_EXIT_WBINVD:  _vmx_default(); break;
	case VMX_EXIT_MOV_CRX: _vmx_mov_crx(); break;
	case VMX_EXIT_MOV_DRX: _vmx_default(); break;
	case VMX_EXIT_XSETBV: _vmx_default(); break;
	case VMX_EXIT_TPR_BELOW_THRESHOLD: _vmx_default(); break;
	case VMX_EXIT_EPT_VIOLATION: _vmx_ept<VMX_EXIT_EPT_VIOLATION>(); break;

	case RECALL:
		recall_wait = Vcpu_handler::_recall_handler();
		if (!recall_wait) {
			_vm_state = RUNNING;
			/* XXX early return for resume */
			_run_vm();
			return;
		}

		/* paused - no resume of vCPU */
		break;

	case VCPU_STARTUP:
		_vmx_startup();

		/* paused - no resume of vCPU */
		break;

	default:
		Genode::error(__func__, " unknown exit - stop - ",
		              Genode::Hex(exit));
		_vm_state = PAUSED;

		/* XXX early return without resume */
		return;
	}

	/* switch to EMT until next vCPU resume */
	Assert(_vm_state != RUNNING);
	_emt.switch_to_emt();

	/* resume vCPU */
	_vm_state = RUNNING;
	if (_next_state == RUN)
		_run_vm();
	else
		_pause_vm(); /* cause pause exit */
}


bool Sup::Vcpu_handler_vmx::_hw_save_state(PVM pVM, PVMCPU pVCpu)
{
	return vmx_save_state(_vcpu.state(), pVM, pVCpu);
}


bool Sup::Vcpu_handler_vmx::_hw_load_state(PVM pVM, PVMCPU pVCpu)
{
	return vmx_load_state(_vcpu.state(), pVM, pVCpu);
}


int Sup::Vcpu_handler_vmx::_vm_exit_requires_instruction_emulation(PCPUMCTX pCtx)
{
	switch (_state->exit_reason) {
	case VMX_EXIT_HLT:
		pCtx->rip++;
		return VINF_EM_HALT;
	case VMX_EXIT_IO_INSTR:
		/* EMHandleRCTmpl.h does not distinguish READ/WRITE rc */
		return VINF_IOM_R3_IOPORT_WRITE;
	case VMX_EXIT_RDMSR:
		return VINF_CPUM_R3_MSR_READ;
	case VMX_EXIT_WRMSR:
		return VINF_CPUM_R3_MSR_WRITE;
	case VMX_EXIT_TPR_BELOW_THRESHOLD:
		/* the instruction causing the exit has already been executed */
	case RECALL:
		return VINF_SUCCESS;
	case VMX_EXIT_EPT_VIOLATION:
		if (_ept_fault_addr_type == PGMPAGETYPE_MMIO)
			/* EMHandleRCTmpl.h does not distinguish READ/WRITE rc */
			return VINF_IOM_R3_MMIO_READ_WRITE;
	case VMX_EXIT_MOV_DRX:
		/* looks complicated in original R0 code -> emulate instead */
		return VINF_EM_RAW_EMULATE_INSTR;
	default:
		return VINF_EM_RAW_EMULATE_INSTR;
	}
}


Sup::Vcpu_handler_vmx::Vcpu_handler_vmx(Genode::Env &env,
                                        unsigned int cpu_id,
                                        Pthread::Emt &emt,
                                        Genode::Vm_connection &vm_connection,
                                        Genode::Allocator &alloc)
:
	Vcpu_handler(env, cpu_id, emt),
	_handler(_emt.genode_ep(), *this, &Vcpu_handler_vmx::_handle_exit),
	_vm_connection(vm_connection),
	_vcpu(_vm_connection, alloc, _handler, _exit_config)
{
	_state = &_vcpu.state();

	/* run vCPU until initial startup exception */
	_vcpu.run();
	_emt.switch_to_vcpu();
}


/*********************
 ** Generic handler **
 *********************/

Genode::Vm_connection::Exit_config const Sup::Vcpu_handler::_exit_config { /* ... */ };


/* TODO move into Emt */
timespec Sup::Vcpu_handler::_add_timespec_ns(timespec a, ::uint64_t ns) const
{
	enum { NSEC_PER_SEC = 1'000'000'000ull };

	long sec = a.tv_sec;

	while (a.tv_nsec >= NSEC_PER_SEC) {
		a.tv_nsec -= NSEC_PER_SEC;
		sec++;
	}
	while (ns >= NSEC_PER_SEC) {
		ns -= NSEC_PER_SEC;
		sec++;
	}

	long nsec = a.tv_nsec + ns;
	while (nsec >= NSEC_PER_SEC) {
		nsec -= NSEC_PER_SEC;
		sec++;
	}
	return timespec { sec, nsec };
}


void Sup::Vcpu_handler::_switch_to_hw(PCPUMCTX pCtx)
{
again:

	/* export FPU state */
	AssertCompile(sizeof(Vcpu_state::Fpu::State) >= sizeof(X86FXSTATE));
	_state->fpu.charge([&] (Vcpu_state::Fpu::State &fpu) {
		::memcpy(&fpu, pCtx->pXStateR3, sizeof(fpu));
	});

	Assert(_vm_state == IRQ_WIN || _vm_state == PAUSED || _vm_state == NPT_EPT);
	Assert(_next_state == PAUSE_EXIT || _next_state == RUN);

	/* run vCPU until next exit */
	_emt.switch_to_vcpu();

	/* next time run - recall() may change this */
	_next_state = RUN;

	/* import FPU state */
	_state->fpu.with_state([&] (Vcpu_state::Fpu::State const &fpu) {
		::memcpy(pCtx->pXStateR3, &fpu, sizeof(X86FXSTATE));
	});

	if (_vm_state == IRQ_WIN) {
		_state->discharge();
		_irq_window_pthread();
		goto again;
	}

	if (!(_vm_state == PAUSED || _vm_state == NPT_EPT))
		Genode::error("which state we are ? ", (int)_vm_state, " ", Genode::Thread::myself()->name());

	Assert(_vm_state == PAUSED || _vm_state == NPT_EPT);
}


void Sup::Vcpu_handler::_default_handler()
{
	if (_vm_state != RUNNING)
		Genode::error(__func__, " _vm_state=", (int)_vm_state, " exit_reason=", Genode::Hex(_state->exit_reason));
	Assert(_vm_state == RUNNING);

	Assert(_state->actv_state.value() == ACTIVITY_STATE_ACTIVE);
	Assert(!(_state->inj_info.value() & IRQ_INJ_VALID_MASK));

	_vm_exits++;

	_vm_state = PAUSED;
}


bool Sup::Vcpu_handler::_recall_handler()
{
	if (_vm_state != RUNNING)
		Genode::error(__func__, " _vm_state=", (int)_vm_state, " exit_reason=", Genode::Hex(_state->exit_reason));
	Assert(_vm_state == RUNNING);

	_vm_exits++;
	_recall_inv++;

	Assert(_state->actv_state.value() == ACTIVITY_STATE_ACTIVE);

	if (_state->inj_info.value() & IRQ_INJ_VALID_MASK) {

		Assert(_state->flags.value() & X86_EFL_IF);

		if (_state->intr_state.value() != INTERRUPT_STATE_NONE)
			Genode::log("intr state ", Genode::Hex(_state->intr_state.value()),
			            " ", Genode::Hex(_state->intr_state.value() & 0xf));

		Assert(_state->intr_state.value() == INTERRUPT_STATE_NONE);

		if (!_continue_hw_accelerated())
			_recall_drop ++;

		/* got recall during irq injection and the guest is ready for
		 * delivery of IRQ - just continue */
		return /* no-wait */ false;
	}

	/* are we forced to go back to emulation mode ? */
	if (!_continue_hw_accelerated()) {
		/* go back to emulation mode */
		_default_handler();
		return /* wait */ true;
	}

	/* check whether we have to request irq injection window */
	if (_check_to_request_irq_window(_vcpu)) {
		_state->discharge();
		_state->inj_info.charge(_state->inj_info.value());
		_irq_win = true;
		return /* no-wait */ false;
	}

	_default_handler();
	return /* wait */ true;
}


bool Sup::Vcpu_handler::_vbox_to_state(VM *pVM, PVMCPU pVCpu)
{
	typedef Genode::Vcpu_state::Range Range;

	PCPUMCTX pCtx  = CPUMQueryGuestCtxPtr(pVCpu);

	_state->ip.charge(pCtx->rip);
	_state->sp.charge(pCtx->rsp);

	_state->ax.charge(pCtx->rax);
	_state->bx.charge(pCtx->rbx);
	_state->cx.charge(pCtx->rcx);
	_state->dx.charge(pCtx->rdx);

	_state->bp.charge(pCtx->rbp);
	_state->si.charge(pCtx->rsi);
	_state->di.charge(pCtx->rdi);

	_state->r8.charge(pCtx->r8);
	_state->r9.charge(pCtx->r9);
	_state->r10.charge(pCtx->r10);
	_state->r11.charge(pCtx->r11);
	_state->r12.charge(pCtx->r12);
	_state->r13.charge(pCtx->r13);
	_state->r14.charge(pCtx->r14);
	_state->r15.charge(pCtx->r15);

	_state->flags.charge(pCtx->rflags.u);

	_state->sysenter_cs.charge(pCtx->SysEnter.cs);
	_state->sysenter_sp.charge(pCtx->SysEnter.esp);
	_state->sysenter_ip.charge(pCtx->SysEnter.eip);

	_state->dr7.charge(pCtx->dr[7]);

	_state->cr0.charge(pCtx->cr0);
	_state->cr2.charge(pCtx->cr2);
	_state->cr3.charge(pCtx->cr3);
	_state->cr4.charge(pCtx->cr4);

	_state->idtr.charge(Range { .limit = pCtx->idtr.cbIdt,
	                            .base  = pCtx->idtr.pIdt });
	_state->gdtr.charge(Range { .limit = pCtx->gdtr.cbGdt,
	                            .base  = pCtx->gdtr.pGdt });

	_state->efer.charge(CPUMGetGuestEFER(pVCpu));

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

		Genode::warning("PDPTE updates disabled!");
//		Genode::uint64_t *pdpte = pdpte_map(pVM, pCtx->cr3);
//
//		_state->pdpte_0.charge(pdpte[0]);
//		_state->pdpte_1.charge(pdpte[1]);
//		_state->pdpte_2.charge(pdpte[2]);
//		_state->pdpte_3.charge(pdpte[3]);
	}

	_state->star.charge(pCtx->msrSTAR);
	_state->lstar.charge(pCtx->msrLSTAR);
	_state->fmask.charge(pCtx->msrSFMASK);
	_state->kernel_gs_base.charge(pCtx->msrKERNELGSBASE);

	/* from HMVMXR0.cpp */
	bool interrupt_pending    = false;
	uint8_t tpr               = 0;
	uint8_t pending_interrupt = 0;
	APICGetTpr(pVCpu, &tpr, &interrupt_pending, &pending_interrupt);

	_state->tpr.charge(tpr);
	_state->tpr_threshold.charge(0);

	if (interrupt_pending) {
		const uint8_t pending_priority = (pending_interrupt >> 4) & 0xf;
		const uint8_t tpr_priority = (tpr >> 4) & 0xf;
		if (pending_priority <= tpr_priority)
			_state->tpr_threshold.charge(pending_priority);
		else
			_state->tpr_threshold.charge(tpr_priority);
	}

	return true;
}


bool Sup::Vcpu_handler::_state_to_vbox(VM *pVM, PVMCPU pVCpu)
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
//	Assert (!(_state->inj_info.value() & IRQ_INJ_VALID_MASK));
	_next_utcb.intr_state = _state->intr_state.value();
	_next_utcb.ctrl[0]    = _state->ctrl_primary.value();
	_next_utcb.ctrl[1]    = _state->ctrl_secondary.value();

	if (_next_utcb.intr_state & 3) {
		_next_utcb.intr_state &= ~3U;
	}

	VMCPU_FF_CLEAR(pVCpu, VMCPU_FF_TO_R3);

	pVCpu->cpum.s.fUseFlags |=  (CPUM_USED_FPU_GUEST);
//	CPUMSetChangedFlags(pVCpu, CPUM_CHANGED_FPU_REM);
//	pVCpu->cpum.s.fUseFlags |=  (CPUM_USED_FPU_GUEST | CPUM_USED_FPU_SINCE_REM);

	if (_state->intr_state.value() != 0) {
		Assert(_state->intr_state.value() == INTERRUPT_STATE_BLOCKING_BY_STI ||
		       _state->intr_state.value() == INTERRUPT_STATE_BLOCKING_BY_MOV_SS);
		EMSetInhibitInterruptsPC(pVCpu, pCtx->rip);
	} else
		VMCPU_FF_CLEAR(pVCpu, VMCPU_FF_INHIBIT_INTERRUPTS);

	APICSetTpr(pVCpu, tpr);

	return true;
}


bool Sup::Vcpu_handler::_check_to_request_irq_window(PVMCPU pVCpu)
{
	if (VMCPU_FF_IS_SET(pVCpu, VMCPU_FF_INHIBIT_INTERRUPTS))
		return false;

	if (!TRPMHasTrap(pVCpu) &&
		!VMCPU_FF_IS_SET(pVCpu, (VMCPU_FF_INTERRUPT_APIC |
		                         VMCPU_FF_INTERRUPT_PIC)))
		return false;

	_irq_request++;

	unsigned const vector = 0;
	_state->inj_info.charge(REQ_IRQWIN_EXIT | vector);

	return true;
}


void Sup::Vcpu_handler::_irq_window()
{
	if (_vm_state != RUNNING)
		Genode::error(__func__, " _vm_state=", (int)_vm_state, " exit_reason=", Genode::Hex(_state->exit_reason));
	Assert(_vm_state == RUNNING);

	_vm_exits++;

	_vm_state = IRQ_WIN;
}


void Sup::Vcpu_handler::_npt_ept()
{
	if (_vm_state != RUNNING)
		Genode::error(__func__, " _vm_state=", (int)_vm_state, " exit_reason=", Genode::Hex(_state->exit_reason));
	Assert(_vm_state == RUNNING);

	_vm_exits++;

	_vm_state = NPT_EPT;
}


void Sup::Vcpu_handler::_irq_window_pthread()
{
	PVMCPU pVCpu = _vcpu;

	Assert(_state->intr_state.value() == INTERRUPT_STATE_NONE);
	Assert(_state->flags.value() & X86_EFL_IF);
	Assert(!VMCPU_FF_IS_SET(pVCpu, VMCPU_FF_INHIBIT_INTERRUPTS));
	Assert(!(_state->inj_info.value() & IRQ_INJ_VALID_MASK));

	Assert(_irq_win);

	_irq_win = false;

	/* request current tpr state from guest, it may block IRQs */
	APICSetTpr(pVCpu, _state->tpr_threshold.value());

	if (!TRPMHasTrap(pVCpu)) {

		bool res = VMCPU_FF_TEST_AND_CLEAR(pVCpu, VMCPU_FF_INTERRUPT_NMI);
		if (res)
			Genode::log("NMI was set");

		if (VMCPU_FF_IS_SET(pVCpu, (VMCPU_FF_INTERRUPT_APIC |
		                            VMCPU_FF_INTERRUPT_PIC))) {

			uint8_t irq;
			int rc = PDMGetInterrupt(pVCpu, &irq);
			Assert(RT_SUCCESS(rc));

			rc = TRPMAssertTrap(pVCpu, irq, TRPM_HARDWARE_INT);
			Assert(RT_SUCCESS(rc));
		}

		if (!TRPMHasTrap(pVCpu)) {
			_irq_drop++;
			/* happens if APICSetTpr (see above) mask IRQ */
			_state->inj_info.charge(IRQ_INJ_NONE);
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
	uint32_t    u32ErrorCode;
	RTGCUINT    cr2;

	Event.u = 0;

	/* If a new event is pending, then dispatch it now. */
	int rc = TRPMQueryTrapAll(pVCpu, &u8Vector, &enmType, &u32ErrorCode, &cr2, 0, 0);
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

	_state->inj_info.charge(Event.u);
	_state->inj_error.charge(Event.n.u32ErrorCode);

	_last_inj_info = _state->inj_info.value();
	_last_inj_error = _state->inj_error.value();

//	Genode::log("type:info:vector ", Genode::Hex(Event.n.u3Type),
//	         Genode::Hex(utcb->inj_info), Genode::Hex(u8Vector),
//	         " intr:actv - ", Genode::Hex(utcb->intr_state),
//	         Genode::Hex(utcb->actv_state), " mtd ",
//	         Genode::Hex(utcb->mtd));
}


bool Sup::Vcpu_handler::_continue_hw_accelerated()
{
	uint32_t check_vm = VM_FF_HM_TO_R3_MASK | VM_FF_REQUEST
	                    | VM_FF_PGM_POOL_FLUSH_PENDING
	                    | VM_FF_PDM_DMA;
	uint32_t check_vcpu = VMCPU_FF_HM_TO_R3_MASK
	                      | VMCPU_FF_PGM_SYNC_CR3
	                      | VMCPU_FF_PGM_SYNC_CR3_NON_GLOBAL
	                      | VMCPU_FF_REQUEST;

	if (!VM_FF_IS_SET(_vm, check_vm) &&
	    !VMCPU_FF_IS_SET(_vcpu, check_vcpu))
		return true;

	Assert(!(VM_FF_IS_SET(_vm, VM_FF_PGM_NO_MEMORY)));

#define VERBOSE_VM(flag) \
	if (VM_FF_IS_SET(_vm, flag)) Genode::log("flag ", flag, " pending")

#define VERBOSE_VMCPU(flag) \
	if (VMCPU_FF_IS_SET(_vcpu, flag)) Genode::log("flag ", flag, " pending")

	if (false) {
		VERBOSE_VM(VM_FF_TM_VIRTUAL_SYNC);
		VERBOSE_VM(VM_FF_PGM_NEED_HANDY_PAGES);
		/* handled by the assertion above */
		//VERBOSE_VM(VM_FF_PGM_NO_MEMORY);
		VERBOSE_VM(VM_FF_PDM_QUEUES);
		VERBOSE_VM(VM_FF_EMT_RENDEZVOUS);
		VERBOSE_VM(VM_FF_REQUEST);
		VERBOSE_VM(VM_FF_PGM_POOL_FLUSH_PENDING);
		VERBOSE_VM(VM_FF_PDM_DMA);

		VERBOSE_VMCPU(VMCPU_FF_TO_R3);
		/* when this flag gets set, a recall request follows */
		//VERBOSE_VMCPU(VMCPU_FF_TIMER);
		VERBOSE_VMCPU(VMCPU_FF_PDM_CRITSECT);
		VERBOSE_VMCPU(VMCPU_FF_PGM_SYNC_CR3);
		VERBOSE_VMCPU(VMCPU_FF_PGM_SYNC_CR3_NON_GLOBAL);
		VERBOSE_VMCPU(VMCPU_FF_REQUEST);
	}

#undef VERBOSE_VMCPU
#undef VERBOSE_VM

	return false;
}


void Sup::Vcpu_handler::recall(VM &vm)
{
	VM *pVM  = &vm;

	if (!_vm || !_vcpu) {
		_vm   = pVM;
		_vcpu = pVM->apCpusR3[_cpu_id];
	}

	if (_vm != pVM || _vcpu != pVM->apCpusR3[_cpu_id])
		Genode::error("wrong CPU !?");

	_recall_req++;

	if (_irq_win) {
		_recall_skip++;
		return;
	}

	asm volatile ("":::"memory");

	if (_vm_state != PAUSED)
		_pause_vm();

	_next_state = PAUSE_EXIT;
}


/* TODO move into Emt */
void Sup::Vcpu_handler::halt(Genode::uint64_t const wait_ns)
{
	/* calculate timeout */
	timespec ts { 0, 0 };
	clock_gettime(CLOCK_REALTIME, &ts);
	ts = _add_timespec_ns(ts, wait_ns);

	/* wait for condition or timeout */
	pthread_mutex_lock(&_mutex);
	pthread_cond_timedwait(&_cond_wait, &_mutex, &ts);
	pthread_mutex_unlock(&_mutex);
}


/* TODO move into Emt */
void Sup::Vcpu_handler::wake_up()
{
	pthread_mutex_lock(&_mutex);
	pthread_cond_signal(&_cond_wait);
	pthread_mutex_unlock(&_mutex);
}


int Sup::Vcpu_handler::run_hw(VM &vm)
{
	VM     * pVM   = &vm;
	PVMCPU   pVCpu = pVM->apCpusR3[_cpu_id];
	PCPUMCTX pCtx  = CPUMQueryGuestCtxPtr(pVCpu);

	if (!_vm || !_vcpu) {
		_vm   = pVM;
		_vcpu = pVM->apCpusR3[_cpu_id];
	}

	if (_vm != pVM || _vcpu != pVM->apCpusR3[_cpu_id])
		Genode::error("wrong CPU !?");

	/* take the utcb state prepared during the last exit */
	_state->inj_info.charge(IRQ_INJ_NONE);
	_state->intr_state.charge(_next_utcb.intr_state);
	_state->actv_state.charge(ACTIVITY_STATE_ACTIVE);
	_state->ctrl_primary.charge(_next_utcb.ctrl[0]);
	_state->ctrl_secondary.charge(_next_utcb.ctrl[1]);

	/* Transfer vCPU state from vbox to Genode format */
	if (!_vbox_to_state(pVM, pVCpu) ||
		!_hw_load_state(pVM, pVCpu)) {

		Genode::error("loading vCPU state failed");
		return VERR_INTERNAL_ERROR;
	}

	/* handle interrupt injection - move to Vcpu_handler */
//	warning(__PRETTY_FUNCTION__, " 1 VMCPU_FF_IS_ANY_SET=",
//	        VMCPU_FF_IS_ANY_SET(pVCpu, VMCPU_FF_INTERRUPT_APIC | VMCPU_FF_UPDATE_APIC | VMCPU_FF_INTERRUPT_PIC
//                                     | VMCPU_FF_INTERRUPT_NMI  | VMCPU_FF_INTERRUPT_SMI));
//	warning(__PRETTY_FUNCTION__, " 2 VM_FF_IS_ANY_SET=",
//	        VM_FF_IS_ANY_SET(pVM, VM_FF_EMT_RENDEZVOUS | VM_FF_TM_VIRTUAL_SYNC),
//	        " VMCPU_FF_IS_ANY_SET=",
//	        VMCPU_FF_IS_ANY_SET(pVCpu, VMCPU_FF_HM_TO_R3_MASK));

	_last_exit_triggered_by_wrmsr = false;

	/* check whether to request interrupt window for injection */
	_irq_win = _check_to_request_irq_window(pVCpu);

	/* mimic state machine implemented in nemHCWinRunGC() etc. */
	VMCPU_SET_STATE(pVCpu, VMCPUSTATE_STARTED_EXEC_NEM);

	/* switch to hardware accelerated mode */
	_switch_to_hw(pCtx);

	Assert(_state->actv_state.value() == ACTIVITY_STATE_ACTIVE);

	/* see hmR0VmxExitToRing3 - sync recompiler state */
	CPUMSetChangedFlags(pVCpu, CPUM_CHANGED_SYSENTER_MSR |
	                    CPUM_CHANGED_LDTR | CPUM_CHANGED_GDTR |
	                    CPUM_CHANGED_IDTR | CPUM_CHANGED_TR |
	                    CPUM_CHANGED_HIDDEN_SEL_REGS |
	                    CPUM_CHANGED_GLOBAL_TLB_FLUSH);

	VMCPU_SET_STATE(pVCpu, VMCPUSTATE_STARTED);

	/* Transfer vCPU state from Genode to vbox format */
	if (!_state_to_vbox(pVM, pVCpu) ||
		!_hw_save_state(pVM, pVCpu)) {

		Genode::error("saving vCPU state failed");
		return VERR_INTERNAL_ERROR;
	}

	/*
	 * Dispatch write to MSR_KVM_SYSTEM_TIME_NEW to emulate
	 * gimR0KvmUpdateSystemTime before entering the gimKvmWriteMsr function.
	 */
	if (_last_exit_triggered_by_wrmsr) {
		enum { MSR_KVM_SYSTEM_TIME_NEW = 0x4b564d01 };
		if (pCtx->ecx == MSR_KVM_SYSTEM_TIME_NEW)
			_update_gim_system_time();
	}

	/* XXX track guest mode changes - see VMM/VMMAll/IEMAllCImpl.cpp.h */
	PGMChangeMode(pVCpu, pCtx->cr0, pCtx->cr4, pCtx->msrEFER);

	int rc = _vm_exit_requires_instruction_emulation(pCtx);

	/* evaluated in VMM/include/EMHandleRCTmpl.h */
	return rc;
}


Sup::Vcpu_handler::Vcpu_handler(Env &env, unsigned int cpu_id, Pthread::Emt &emt)
:
	_emt(emt), _cpu_id(cpu_id)
{
	pthread_mutexattr_t _attr;
	pthread_mutexattr_init(&_attr);

	pthread_cond_init(&_cond_wait, nullptr);

	pthread_mutexattr_settype(&_attr, PTHREAD_MUTEX_ERRORCHECK);
	pthread_mutex_init(&_mutex, &_attr);
}
