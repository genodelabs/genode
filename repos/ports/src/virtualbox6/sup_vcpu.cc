/*
 * \brief  SUPLib vCPU utility
 * \author Alexander Boettcher
 * \author Norman Feske
 * \author Christian Helmuth
 * \author Benjamin Lamowski
 */

/*
 * Copyright (C) 2013-2023 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

/* Genode includes */
#include <base/attached_dataspace.h>
#include <vm_session/handler.h>
#include <util/noncopyable.h>
#include <cpu/vcpu_state.h>
#include <cpu/memory_barrier.h>
#include <libc/allocator.h>

/* VirtualBox includes */
#include <VBox/vmm/cpum.h> /* must be included before CPUMInternal.h */
#include <CPUMInternal.h>  /* enable access to cpum.s.* */
#include <HMInternal.h>    /* enable access to hm.s.* */
#include <PGMInternal.h>   /* enable access to pgm.s.* */
#include <VBox/vmm/vmcc.h>  /* must be included before PGMInline.h */
#include <PGMInline.h>     /* pgmPhysGetRangeAtOrAbove() */
#include <VBox/vmm/vm.h>
#include <VBox/vmm/hm.h>
#include <VBox/vmm/apic.h>
#include <VBox/vmm/em.h>
#include <VBox/vmm/pdmapi.h>
#include <VBox/err.h>
#include <iprt/time.h>
#include <iprt/semaphore.h>

/* libc includes */
#include <stdlib.h> /* for exit() */
#include <pthread.h>
#include <errno.h>

/* local includes */
#include <sup_vcpu.h>
#include <pthread_emt.h>

using namespace Genode;


/*
 * VirtualBox stores segment attributes in Intel format using 17 bits of a
 * 32-bit value, which includes bits 19:16 of segment limit (see
 * X86DESCATTRBITS).
 *
 * Genode represents the attributes in packed SVM VMCB format using 13 bits of
 * a 16-bit value without segment-limit bits.
 */
static inline uint16_t sel_ar_conv_to_genode(Genode::uint32_t v)
{
	return (v & 0xff) | ((v & 0x1f000) >> 4);
}


static inline Genode::uint32_t sel_ar_conv_from_genode(Genode::uint16_t v)
{
	return (v & 0xff) | (((uint32_t )v << 4) & 0x1f000);
}

namespace Sup {

	struct Vmx;
	struct Svm;

	enum class Exit_state { DEFAULT, NPT_EPT, PAUSED, IRQ_WINDOW, STARTUP, ERROR };

	struct Handle_exit_result
	{
		Exit_state   state;
		VBOXSTRICTRC rc;
	};

	template <typename> struct Vcpu_impl;
}

#include <sup_vcpu_vmx.h>
#include <sup_vcpu_svm.h>


template <typename VIRT>
class Sup::Vcpu_impl : public Sup::Vcpu, Genode::Noncopyable
{
	public:

		struct State_container { Vcpu_state &ref; };

	private:

		Pthread::Emt    &_emt;
		Cpu_index const  _cpu;
		VM              &_vm;
		VMCPU           &_vmcpu;
		Libc::Allocator  _alloc;

		Genode::Constructible<State_container> _state;

		/* exit handler run in vCPU mode - switches to EMT */
		void _handle_exit();

		Vcpu_handler<Vcpu_impl<VIRT>> _handler {
			_emt.genode_ep(), *this, &Vcpu_impl<VIRT>::_handle_exit };

		Vm_connection::Vcpu _vcpu;

		/* halt/wake_up support */
		RTSEMEVENTMULTI _halt_semevent { NIL_RTSEMEVENTMULTI };

		/* state machine between EMT and vCPU mode */
		enum Current_state { RUNNING, PAUSED } _current_state { PAUSED };

		Genode::Mutex _nem_guard { };
		bool _check_force_flags = false;

		/* interrupt-window exit requested */
		bool _irq_window = false;

		enum {
			REQ_IRQ_WINDOW_EXIT           = 0x1000U,
			VMX_ENTRY_INT_INFO_NONE       = 0U,
			VMX_VMCS_GUEST_INT_STATE_NONE = 0U,
		};

		struct {
			unsigned ctrl_primary   = VIRT::ctrl_primary();
			unsigned ctrl_secondary = VIRT::ctrl_secondary();
		} _cached_state;

		inline void _transfer_state_to_vcpu(CPUMCTX const &);
		inline void _transfer_state_to_vbox(CPUMCTX &);

		inline bool _check_and_request_irq_window();
		inline bool _continue_hw_accelerated();

		inline VBOXSTRICTRC _switch_to_hw();

		inline Current_state _handle_npt_ept(VBOXSTRICTRC &);
		inline Current_state _handle_paused();
		inline Current_state _handle_irq_window();
		inline Current_state _handle_startup();

	public:

		Vcpu_impl(Genode::Env &, VM &, Vm_connection &, Cpu_index, Pthread::Emt &);

		/* Vcpu interface */

		VBOXSTRICTRC run() override;

		void pause() override;

		void halt(Genode::uint64_t const wait_ns) override;

		void wake_up() override;
};


template <typename T> void Sup::Vcpu_impl<T>::_handle_exit()
{
	_vcpu.with_state([this](Genode::Vcpu_state &state) {
		_state.construct(state);
		_emt.switch_to_emt();
		_state.destruct();
		return true;
	});
}


template <typename VIRT> void Sup::Vcpu_impl<VIRT>::_transfer_state_to_vcpu(CPUMCTX const &ctx)
{
	Vcpu_state &state { _state->ref };

	/* transfer defaults and cached state */
	state.ctrl_primary.charge(_cached_state.ctrl_primary); /* XXX always updates ctrls */
	state.ctrl_secondary.charge(_cached_state.ctrl_secondary); /* XXX always updates ctrls */

	typedef Genode::Vcpu_state::Range Range;

	state.ip.charge(ctx.rip);
	state.sp.charge(ctx.rsp);

	state.ax.charge(ctx.rax);
	state.bx.charge(ctx.rbx);
	state.cx.charge(ctx.rcx);
	state.dx.charge(ctx.rdx);

	state.bp.charge(ctx.rbp);
	state.si.charge(ctx.rsi);
	state.di.charge(ctx.rdi);

	state.r8.charge(ctx.r8);
	state.r9.charge(ctx.r9);
	state.r10.charge(ctx.r10);
	state.r11.charge(ctx.r11);
	state.r12.charge(ctx.r12);
	state.r13.charge(ctx.r13);
	state.r14.charge(ctx.r14);
	state.r15.charge(ctx.r15);

	state.flags.charge(ctx.rflags.u);

	state.sysenter_cs.charge(ctx.SysEnter.cs);
	state.sysenter_sp.charge(ctx.SysEnter.esp);
	state.sysenter_ip.charge(ctx.SysEnter.eip);

	state.dr7.charge(ctx.dr[7]);

	state.cr0.charge(ctx.cr0);
	state.cr2.charge(ctx.cr2);
	state.cr3.charge(ctx.cr3);
	state.cr4.charge(ctx.cr4);

	state.idtr.charge(Range { .limit = ctx.idtr.cbIdt,
	                          .base  = ctx.idtr.pIdt });
	state.gdtr.charge(Range { .limit = ctx.gdtr.cbGdt,
	                          .base  = ctx.gdtr.pGdt });

	state.efer.charge(CPUMGetGuestEFER(&_vmcpu));

	/*
	 * Update the PDPTE registers if necessary
	 *
	 * Intel manual sections 4.4.1 of Vol. 3A and 26.3.2.4 of Vol. 3C
	 * indicate the conditions when this is the case. The following
	 * code currently does not check if the recompiler modified any
	 * CR registers, which means the update can happen more often
	 * than really necessary.
	 */
	if (_vm.hm.s.vmx.fSupported &&
		CPUMIsGuestPagingEnabledEx(&ctx) &&
		CPUMIsGuestInPAEModeEx(&ctx)) {

		Genode::warning("PDPTE updates disabled!");
	}

	state.star.charge(ctx.msrSTAR);
	state.lstar.charge(ctx.msrLSTAR);
	state.cstar.charge(ctx.msrCSTAR);
	state.fmask.charge(ctx.msrSFMASK);
	state.kernel_gs_base.charge(ctx.msrKERNELGSBASE);

	/* from HMVMXR0.cpp */
	bool interrupt_pending    = false;
	uint8_t tpr               = 0;
	uint8_t pending_interrupt = 0;
	APICGetTpr(&_vmcpu, &tpr, &interrupt_pending, &pending_interrupt);

	state.tpr.charge(tpr);
	state.tpr_threshold.charge(0);

	if (interrupt_pending) {
		const uint8_t pending_priority = (pending_interrupt >> 4) & 0xf;
		const uint8_t tpr_priority = (tpr >> 4) & 0xf;
		if (pending_priority <= tpr_priority)
			state.tpr_threshold.charge(pending_priority);
	}

	/* export FPU state - start */
	state.xcr0.charge(ctx.aXcr[0]);

	{
		::uint64_t ia32_xss = 0;
		auto const rc = CPUMQueryGuestMsr(&_vmcpu, 0xDA0 /* MSR_IA32_XSS */,
		                                  &ia32_xss);

		if (rc == VINF_SUCCESS)
			state.xss.charge(ia32_xss);
	}

	_state->ref.fpu.charge([&](Vcpu_state::Fpu::State &fpu) {
		unsigned fpu_size = min(_vm.cpum.s.HostFeatures.cbMaxExtendedState,
		                        sizeof(fpu._buffer));

		::memcpy(fpu._buffer, ctx.pXStateR3, fpu_size);

		return fpu_size;
	});
	/* export FPU state - end */

	{
		::uint64_t tsc_aux = 0;
		auto const rcStrict = CPUMQueryGuestMsr(&_vmcpu, MSR_K8_TSC_AUX,
		                                        &tsc_aux);
		Assert(rcStrict == VINF_SUCCESS);
		if (rcStrict == VINF_SUCCESS)
			state.tsc_aux.charge(tsc_aux);
	}

	/* do SVM/VMX-specific transfers */
	VIRT::transfer_state_to_vcpu(state, ctx);
}


/*
 * Based on hmR0VmxImportGuestIntrState()
 */
static void handle_intr_state(PVMCPUCC pVCpu, CPUMCTX &ctx, Vcpu_state &state)
{
	auto const interrupt_state = state.intr_state.value();

	if (!interrupt_state /* VMX_VMCS_GUEST_INT_STATE_NONE */) {
		if (VMCPU_FF_IS_SET(pVCpu, VMCPU_FF_INHIBIT_INTERRUPTS))
			VMCPU_FF_CLEAR(pVCpu, VMCPU_FF_INHIBIT_INTERRUPTS);
		CPUMSetGuestNmiBlocking(pVCpu, false);
	} else {
		if (interrupt_state & (VMX_VMCS_GUEST_INT_STATE_BLOCK_MOVSS |
		                       VMX_VMCS_GUEST_INT_STATE_BLOCK_STI))
			EMSetInhibitInterruptsPC(pVCpu, ctx.rip);
		else if (VMCPU_FF_IS_SET(pVCpu, VMCPU_FF_INHIBIT_INTERRUPTS))
			VMCPU_FF_CLEAR(pVCpu, VMCPU_FF_INHIBIT_INTERRUPTS);

		bool const block_nmi = RT_BOOL(interrupt_state &
		                               VMX_VMCS_GUEST_INT_STATE_BLOCK_NMI);
		CPUMSetGuestNmiBlocking(pVCpu, block_nmi);
	}

	/* prepare clearing blocking MOV SS or STI bits for next VM-entry */
	if (interrupt_state & (VMX_VMCS_GUEST_INT_STATE_BLOCK_MOVSS |
	                       VMX_VMCS_GUEST_INT_STATE_BLOCK_STI)) {
		state.intr_state.charge(state.intr_state.value() &
		                        ~unsigned(VMX_VMCS_GUEST_INT_STATE_BLOCK_MOVSS |
		                                   VMX_VMCS_GUEST_INT_STATE_BLOCK_STI));
		state.actv_state.charge(VMX_VMCS_GUEST_ACTIVITY_ACTIVE);
	}
}


template <typename VIRT> void Sup::Vcpu_impl<VIRT>::_transfer_state_to_vbox(CPUMCTX &ctx)
{
	Vcpu_state const &state { _state->ref };

	ctx.rip = state.ip.value();
	ctx.rsp = state.sp.value();

	ctx.rax = state.ax.value();
	ctx.rbx = state.bx.value();
	ctx.rcx = state.cx.value();
	ctx.rdx = state.dx.value();

	ctx.rbp = state.bp.value();
	ctx.rsi = state.si.value();
	ctx.rdi = state.di.value();
	ctx.rflags.u = state.flags.value();

	ctx.r8  = state.r8.value();
	ctx.r9  = state.r9.value();
	ctx.r10 = state.r10.value();
	ctx.r11 = state.r11.value();
	ctx.r12 = state.r12.value();
	ctx.r13 = state.r13.value();
	ctx.r14 = state.r14.value();
	ctx.r15 = state.r15.value();

	ctx.dr[7] = state.dr7.value();

	PVMCPU pVCpu = &_vmcpu;

	if (ctx.SysEnter.cs != state.sysenter_cs.value())
		CPUMSetGuestMsr(pVCpu, MSR_IA32_SYSENTER_CS, state.sysenter_cs.value());

	if (ctx.SysEnter.esp != state.sysenter_sp.value())
		CPUMSetGuestMsr(pVCpu, MSR_IA32_SYSENTER_ESP, state.sysenter_sp.value());

	if (ctx.SysEnter.eip != state.sysenter_ip.value())
		CPUMSetGuestMsr(pVCpu, MSR_IA32_SYSENTER_EIP, state.sysenter_ip.value());

	if (ctx.idtr.cbIdt != state.idtr.value().limit ||
	    ctx.idtr.pIdt  != state.idtr.value().base)
		CPUMSetGuestIDTR(pVCpu, state.idtr.value().base, state.idtr.value().limit);

	if (ctx.gdtr.cbGdt != state.gdtr.value().limit ||
	    ctx.gdtr.pGdt  != state.gdtr.value().base)
		CPUMSetGuestGDTR(pVCpu, state.gdtr.value().base, state.gdtr.value().limit);

	CPUMSetGuestEFER(pVCpu, state.efer.value());

	if (ctx.cr0 != state.cr0.value())
		CPUMSetGuestCR0(pVCpu, state.cr0.value());

	if (ctx.cr2 != state.cr2.value())
		CPUMSetGuestCR2(pVCpu, state.cr2.value());

	if (ctx.cr3 != state.cr3.value()) {
		CPUMSetGuestCR3(pVCpu, state.cr3.value());
		VMCPU_FF_SET(pVCpu, VMCPU_FF_HM_UPDATE_CR3);
	}

	if (ctx.cr4 != state.cr4.value())
		CPUMSetGuestCR4(pVCpu, state.cr4.value());

	if (ctx.msrSTAR != state.star.value())
		CPUMSetGuestMsr(pVCpu, MSR_K6_STAR, state.star.value());

	if (ctx.msrLSTAR != state.lstar.value())
		CPUMSetGuestMsr(pVCpu, MSR_K8_LSTAR, state.lstar.value());

	if (ctx.msrCSTAR != state.cstar.value())
		CPUMSetGuestMsr(pVCpu, MSR_K8_CSTAR, state.cstar.value());

	if (ctx.msrSFMASK != state.fmask.value())
		CPUMSetGuestMsr(pVCpu, MSR_K8_SF_MASK, state.fmask.value());

	if (ctx.msrKERNELGSBASE != state.kernel_gs_base.value())
		CPUMSetGuestMsr(pVCpu, MSR_K8_KERNEL_GS_BASE, state.kernel_gs_base.value());

	uint32_t const tpr = state.tpr.value();

	/* update cached state */
	_cached_state.ctrl_primary   = state.ctrl_primary.value();
	_cached_state.ctrl_secondary = state.ctrl_secondary.value();

	/* handle guest interrupt state */
	handle_intr_state(pVCpu, ctx, _state->ref);

	VMCPU_FF_CLEAR(pVCpu, VMCPU_FF_TO_R3);

	_vmcpu.cpum.s.fUseFlags |= CPUM_USED_FPU_GUEST;

	APICSetTpr(pVCpu, tpr);

	/* import FPU state - start */
	_state->ref.fpu.with_state([&](Vcpu_state::Fpu::State const &fpu) {
		unsigned fpu_size = min(_vm.cpum.s.HostFeatures.cbMaxExtendedState,
		                        sizeof(fpu._buffer));

		::memcpy(ctx.pXStateR3, fpu._buffer, fpu_size);

		return true;
	});

	CPUMSetGuestMsr (pVCpu, 0xDA0 /* MSR_IA32_XSS */, state.xss.value());
	CPUMSetGuestXcr0(pVCpu, state.xcr0.value());

	/* import FPU state - end */

	/* do SVM/VMX-specific transfers */
	VIRT::transfer_state_to_vbox(state, _vmcpu, ctx);
}


template <typename T> bool Sup::Vcpu_impl<T>::_check_and_request_irq_window()
{
	PVMCPU pVCpu = &_vmcpu;

	if (VMCPU_FF_TEST_AND_CLEAR(pVCpu, VMCPU_FF_UPDATE_APIC))
		APICUpdatePendingInterrupts(pVCpu);

	if (VMCPU_FF_IS_SET(pVCpu, VMCPU_FF_INHIBIT_INTERRUPTS))
		return false;

	if (!TRPMHasTrap(pVCpu) &&
		!VMCPU_FF_IS_ANY_SET(pVCpu, (VMCPU_FF_INTERRUPT_APIC |
		                             VMCPU_FF_INTERRUPT_PIC)))
		return false;

	_state->ref.inj_info.charge(REQ_IRQ_WINDOW_EXIT);

	return true;
}


template <typename T> bool Sup::Vcpu_impl<T>::_continue_hw_accelerated()
{
	::uint32_t check_vm = VM_FF_HM_TO_R3_MASK
	                    | VM_FF_REQUEST
	                    | VM_FF_PGM_POOL_FLUSH_PENDING
	                    | VM_FF_PDM_DMA;
	/* VMCPU_WITH_64_BIT_FFS is enabled */
	::uint64_t check_vmcpu = VMCPU_FF_HM_TO_R3_MASK
	                       | VMCPU_FF_PGM_SYNC_CR3
	                       | VMCPU_FF_PGM_SYNC_CR3_NON_GLOBAL
	                       | VMCPU_FF_REQUEST
	                       | VMCPU_FF_TIMER;

	if (!VM_FF_IS_ANY_SET(&_vm, check_vm) &&
	    !VMCPU_FF_IS_ANY_SET(&_vmcpu, check_vmcpu))
		return true;

	Assert(!(VM_FF_IS_SET(&_vm, VM_FF_PGM_NO_MEMORY)));

#define VERBOSE_VM(flag) \
	if (VM_FF_IS_SET(&_vm, flag)) LogAlways(("flag %s (%x) pending\n", #flag, flag))

#define VERBOSE_VMCPU(flag) \
	if (VMCPU_FF_IS_SET(&_vmcpu, flag)) LogAlways(("flag %s (%llx) pending\n", #flag, flag))

	if (false && VM_FF_IS_ANY_SET(&_vm, check_vm)) {
		LogAlways(("VM_FF=%x\n", _vm.fGlobalForcedActions));
		VERBOSE_VM(VM_FF_TM_VIRTUAL_SYNC);
		VERBOSE_VM(VM_FF_PGM_NEED_HANDY_PAGES);
		/* handled by the assertion above
		VERBOSE_VM(VM_FF_PGM_NO_MEMORY); */
		VERBOSE_VM(VM_FF_PDM_QUEUES);
		VERBOSE_VM(VM_FF_EMT_RENDEZVOUS);
		VERBOSE_VM(VM_FF_REQUEST);
		VERBOSE_VM(VM_FF_PGM_POOL_FLUSH_PENDING);
		VERBOSE_VM(VM_FF_PDM_DMA);
	}
	if (false && VMCPU_FF_IS_ANY_SET(&_vmcpu, check_vmcpu)) {
		LogAlways(("VMCPU_FF=%llx\n", _vmcpu.fLocalForcedActions));
		VERBOSE_VMCPU(VMCPU_FF_TO_R3);
		VERBOSE_VMCPU(VMCPU_FF_PDM_CRITSECT);
		VERBOSE_VMCPU(VMCPU_FF_PGM_SYNC_CR3);
		VERBOSE_VMCPU(VMCPU_FF_PGM_SYNC_CR3_NON_GLOBAL);
		VERBOSE_VMCPU(VMCPU_FF_REQUEST);
	}

#undef VERBOSE_VMCPU
#undef VERBOSE_VM

	return false;
}


template <typename T>
typename Sup::Vcpu_impl<T>::Current_state Sup::Vcpu_impl<T>::_handle_npt_ept(VBOXSTRICTRC &rc)
{
	rc = VINF_EM_RAW_EMULATE_INSTR;

	RTGCPHYS const GCPhys = PAGE_ADDRESS(_state->ref.qual_secondary.value());

	PPGMRAMRANGE const pRam = pgmPhysGetRangeAtOrAbove(&_vm, GCPhys);
	if (!pRam)
		return PAUSED;

	RTGCPHYS const off = GCPhys - pRam->GCPhys;
	if (off >= pRam->cb)
		return PAUSED;

	unsigned const iPage = off >> PAGE_SHIFT;
	PPGMPAGE const pPage = &pRam->aPages[iPage];

	/* EMHandleRCTmpl.h does not distinguish READ/WRITE rc */
	if (PGM_PAGE_GET_TYPE(pPage) == PGMPAGETYPE_MMIO)
		rc = VINF_IOM_R3_MMIO_READ_WRITE;

	return PAUSED;
}


template <typename T>
typename Sup::Vcpu_impl<T>::Current_state Sup::Vcpu_impl<T>::_handle_paused()
{
	Vcpu_state &state { _state->ref };

	Assert(state.actv_state.value() == VMX_VMCS_GUEST_ACTIVITY_ACTIVE);

	if (VMX_EXIT_INT_INFO_IS_VALID(state.inj_info.value())) {

		Assert(state.flags.value() & X86_EFL_IF);

		/*
		 * We got a pause exit during IRQ injection and the guest is ready for
		 * IRQ injection. So, just continue running the vCPU.
		 */
		return RUNNING;
	}

	/* are we forced to go back to emulation mode ? */
	if (!_continue_hw_accelerated()) {
		/* go back to emulation mode */
		return PAUSED;
	}

	/* check whether we have to request irq injection window */
	if (_check_and_request_irq_window()) {
		state.inj_info.charge(state.inj_info.value());
		_irq_window = true;
		return RUNNING;
	}

	return PAUSED;
}


template <typename T>
typename Sup::Vcpu_impl<T>::Current_state Sup::Vcpu_impl<T>::_handle_startup()
{
	return PAUSED;
}


template <typename T>
typename Sup::Vcpu_impl<T>::Current_state Sup::Vcpu_impl<T>::_handle_irq_window()
{
	Vcpu_state &state { _state->ref };

	PVMCPU pVCpu = &_vmcpu;

	Assert(state.flags.value() & X86_EFL_IF);
	Assert(!VMCPU_FF_IS_SET(pVCpu, VMCPU_FF_INHIBIT_INTERRUPTS));
	Assert(!VMX_EXIT_INT_INFO_IS_VALID(state.inj_info.value()));
	Assert(_irq_window);

	_irq_window = false;

	/* request current tpr state from guest, it may block IRQs */
	APICSetTpr(pVCpu, state.tpr.value());

	if (!TRPMHasTrap(pVCpu)) {

		bool res = VMCPU_FF_TEST_AND_CLEAR(pVCpu, VMCPU_FF_INTERRUPT_NMI);
		if (res)
			warning("NMI was set");

		if (VMCPU_FF_IS_SET(pVCpu, (VMCPU_FF_INTERRUPT_APIC |
		                            VMCPU_FF_INTERRUPT_PIC))) {

			uint8_t irq;
			int rc = PDMGetInterrupt(pVCpu, &irq);
			if (RT_SUCCESS(rc)) {
				rc = TRPMAssertTrap(pVCpu, irq, TRPM_HARDWARE_INT);
				Assert(RT_SUCCESS(rc));
			} else if (rc == VERR_APIC_INTR_MASKED_BY_TPR) {
				state.tpr_threshold.charge(irq >> 4);
			}
		}

		if (!TRPMHasTrap(pVCpu)) {
			/* happens if APICSetTpr (see above) mask IRQ */
			state.inj_info.charge(VMX_ENTRY_INT_INFO_NONE);
			return PAUSED;
		}
	}

	/*
	 * If we have no IRQ for injection, something with requesting the
	 * IRQ window went wrong. Probably it was forgotten to be reset.
	 */
	Assert(TRPMHasTrap(pVCpu));

	/* interrupt can be dispatched */
	uint8_t   u8Vector   { };
	TRPMEVENT event_type { TRPM_HARDWARE_INT };
	SVMEVENT  event      { };
	uint32_t  errorcode  { };
	RTGCUINT  cr2        { };

	/* If a new event is pending, then dispatch it now. */
	int rc = TRPMQueryTrapAll(pVCpu, &u8Vector, &event_type, &errorcode, &cr2, 0, 0);
	AssertRC(rc);
	if (rc != VINF_SUCCESS) {
		Genode::warning("no trap available");
		return RUNNING;
	}

	/* based upon hmR0SvmTrpmTrapToPendingEvent */
	switch (event_type) {
	case TRPM_TRAP:
		event.n.u1Valid  = 1;
		event.n.u8Vector = u8Vector;

		switch (u8Vector) {
			case X86_XCPT_NMI:
				event.n.u3Type = SVM_EVENT_NMI;

				static_assert(SVM_EVENT_NMI == VMX_ENTRY_INT_INFO_TYPE_NMI,
				              "SVM vs VMX mismatch");
				break;
			default:
				Genode::error("unsupported injection case - "
				              "TRPM_TRAP, vector=", u8Vector);
				Assert(!"unsupported injection case");
				return PAUSED;
		}
		break;
	case TRPM_HARDWARE_INT:
		event.n.u1Valid  = 1;
		event.n.u8Vector = u8Vector;
		event.n.u3Type   = SVM_EVENT_EXTERNAL_IRQ;

		static_assert(VMX_ENTRY_INT_INFO_TYPE_EXT_INT == SVM_EVENT_EXTERNAL_IRQ,
		              "SVM vs VMX mismatch");

		break;
	case TRPM_SOFTWARE_INT:
		event.n.u1Valid  = 1;
		event.n.u8Vector = u8Vector;
		event.n.u3Type = SVM_EVENT_SOFTWARE_INT;

		static_assert(VMX_ENTRY_INT_INFO_TYPE_SW_INT == SVM_EVENT_SOFTWARE_INT,
		              "SVM vs VMX mismatch");
	default:
		Genode::error("unsupported injection case");
		Assert(!"unsupported injection case");
		return PAUSED;
	}

	/* Clear the pending trap. */
	rc = TRPMResetTrap(pVCpu);
	AssertRC(rc);

	state.inj_info.charge(event.u);
	state.inj_error.charge(errorcode);

	return RUNNING;
}


template <typename VIRT> VBOXSTRICTRC Sup::Vcpu_impl<VIRT>::_switch_to_hw()
{
	Handle_exit_result result;
	do {
		_current_state = RUNNING;

		/* run vCPU until next exit */
		_emt.switch_to_vcpu();

		result = VIRT::handle_exit(_state->ref);

		/* discharge by default */
		_state->ref.discharge();

		switch (result.state) {

		case Exit_state::STARTUP:
			_current_state = _handle_startup();
			break;

		case Exit_state::IRQ_WINDOW:
			_current_state = _handle_irq_window();
			break;

		case Exit_state::PAUSED:
			_current_state = _handle_paused();
			break;

		case Exit_state::NPT_EPT:
			_current_state = _handle_npt_ept(result.rc);
			break;

		case Exit_state::DEFAULT:
		case Exit_state::ERROR:
			_current_state = PAUSED;
			break;
		}

	} while (_current_state == RUNNING);

	return result.rc;
}


/********************
 ** Vcpu interface **
 ********************/

template <typename T> void Sup::Vcpu_impl<T>::halt(Genode::uint64_t const wait_ns)
{
	/* always wait for at least 1 ms */
	Genode::uint64_t const v = wait_ns / RT_NS_1MS ? : 1;

	RTSemEventMultiWait(_halt_semevent, v);
	RTSemEventMultiReset(_halt_semevent);
}


template <typename T> void Sup::Vcpu_impl<T>::wake_up()
{
	RTSemEventMultiSignal(_halt_semevent);
}


template <typename T> void Sup::Vcpu_impl<T>::pause()
{
	Genode::Mutex::Guard guard(_nem_guard);

	PVMCPU     pVCpu    = &_vmcpu;
	VMCPUSTATE enmState = pVCpu->enmState;

	if (enmState == VMCPUSTATE_STARTED_EXEC_NEM)
		_handler.local_submit();
	else
		_check_force_flags = true;
}


template <typename T> VBOXSTRICTRC Sup::Vcpu_impl<T>::run()
{
	PVMCPU   pVCpu = &_vmcpu;
	CPUMCTX &ctx   = *CPUMQueryGuestCtxPtr(pVCpu);

	{
		Genode::Mutex::Guard guard(_nem_guard);

		if (_check_force_flags) {
			_check_force_flags = false;
			if (!Sup::Vcpu_impl<T>::_continue_hw_accelerated())
				return VINF_SUCCESS;
		}

		/* mimic state machine implemented in nemHCWinRunGC() etc. */
		VMCPU_SET_STATE(pVCpu, VMCPUSTATE_STARTED_EXEC_NEM);
	}

	_transfer_state_to_vcpu(ctx);

	/* XXX move this into _transfer_state_to_vcpu ? */
	/* check whether to request interrupt window for injection */
	_irq_window = _check_and_request_irq_window();

	VBOXSTRICTRC const rc = _switch_to_hw();

	_transfer_state_to_vbox(ctx);

	Assert(_state->ref.actv_state.value() == VMX_VMCS_GUEST_ACTIVITY_ACTIVE);

	/* see hmR0VmxExitToRing3 - sync recompiler state */
	CPUMSetChangedFlags(pVCpu, CPUM_CHANGED_SYSENTER_MSR |
	                    CPUM_CHANGED_LDTR | CPUM_CHANGED_GDTR |
	                    CPUM_CHANGED_IDTR | CPUM_CHANGED_TR |
	                    CPUM_CHANGED_HIDDEN_SEL_REGS |
	                    CPUM_CHANGED_GLOBAL_TLB_FLUSH);

	/* mimic state machine implemented in nemHCWinRunGC() etc. */
	VMCPU_SET_STATE(pVCpu, VMCPUSTATE_STARTED);

	/*
	 * Dispatch write to MSR_KVM_SYSTEM_TIME_NEW to emulate
	 * gimR0KvmUpdateSystemTime before entering the gimKvmWriteMsr function.
	 */
	if (rc == VINF_CPUM_R3_MSR_WRITE) {
		enum { MSR_KVM_SYSTEM_TIME_NEW = 0x4b564d01 };
		if (ctx.ecx == MSR_KVM_SYSTEM_TIME_NEW)
			Sup::update_gim_system_time(_vm, _vmcpu);
	}

	/* track guest mode changes - see VMM/VMMAll/IEMAllCImpl.cpp.h */
	PGMChangeMode(pVCpu, ctx.cr0, ctx.cr4, ctx.msrEFER);

	/* avoid assertion in EMHandleRCTmpl.h, normally set by SVMRO/VMXR0 */
	if (TRPMHasTrap(pVCpu))
		return VINF_EM_RAW_INJECT_TRPM_EVENT;

	/* evaluated in VMM/include/EMHandleRCTmpl.h */
	return rc;
}


template <typename VIRT>
Sup::Vcpu_impl<VIRT>::Vcpu_impl(Env &env, VM &vm, Vm_connection &vm_con,
                                Cpu_index cpu, Pthread::Emt &emt)
:
	_emt(emt), _cpu(cpu), _vm(vm), _vmcpu(*vm.apCpusR3[cpu.value]),
	_vcpu(vm_con, _alloc, _handler, VIRT::exit_config)
{
	RTSemEventMultiCreate(&_halt_semevent);

	/* run vCPU until initial startup exception */
	_switch_to_hw();
}


/*****************************
 ** vCPU creation functions **
 *****************************/

Sup::Vcpu & Sup::Vcpu::create_svm(Genode::Env &env, VM &vm, Vm_connection &vm_con,
                                  Cpu_index cpu, Pthread::Emt &emt)
{
	return *new Vcpu_impl<Svm>(env, vm, vm_con, cpu, emt);
}


Sup::Vcpu & Sup::Vcpu::create_vmx(Genode::Env &env, VM &vm, Vm_connection &vm_con,
                                  Cpu_index cpu, Pthread::Emt &emt)
{
	return *new Vcpu_impl<Vmx>(env, vm, vm_con, cpu, emt);
}
