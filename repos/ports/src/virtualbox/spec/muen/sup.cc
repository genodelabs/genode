/*
 * \brief  Genode specific VirtualBox SUPLib supplements.
 * \author Alexander Boettcher
 * \author Adrian-Ken Rueegsegger
 * \author Reto Buerki
 * \date   2013-11-18
 */

/*
 * Copyright (C) 2013-2017 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

/* Genode includes */
#include <base/log.h>
#include <timer_session/connection.h>
#include <base/attached_io_mem_dataspace.h>
#include <rom_session/connection.h>
#include <muen/sinfo.h>

/* VirtualBox includes */
#include "HMInternal.h" /* enable access to hm.s.* */
#include "CPUMInternal.h" /* enable access to cpum.s.* */
#include <VBox/vmm/rem.h>
#include <VBox/vmm/pdmapi.h>
#include <VBox/vmm/hm_vmx.h>

/* Genode's VirtualBox includes */
#include "sup.h"
#include "vcpu.h"
#include "vm_handler.h"
#include "vmm_memory.h"
#include "guest_interrupts.h"

/* Libc include */
#include <pthread.h>

enum {
	VMCS_SEG_UNUSABLE    = 0x10000,
	INTERRUPT_STATE_NONE = 0U,
	BLOCKING_BY_STI      = 1U << 0,
	BLOCKING_BY_MOV_SS   = 1U << 1,
};

#define GENODE_READ_SELREG_REQUIRED(REG) \
    (pCtx->REG.Sel != cur_state->REG.sel) || \
    (pCtx->REG.ValidSel != cur_state->REG.sel) || \
    (pCtx->REG.fFlags   != CPUMSELREG_FLAGS_VALID) || \
    (pCtx->REG.u32Limit != cur_state->REG.limit) || \
    (pCtx->REG.u64Base  != cur_state->REG.base) || \
    (pCtx->REG.Attr.u   != cur_state->REG.access)

#define GENODE_READ_SELREG(REG) \
	pCtx->REG.Sel      = cur_state->REG.sel; \
	pCtx->REG.ValidSel = cur_state->REG.sel; \
	pCtx->REG.fFlags   = CPUMSELREG_FLAGS_VALID; \
	pCtx->REG.u32Limit = cur_state->REG.limit; \
	pCtx->REG.u64Base  = cur_state->REG.base; \
	pCtx->REG.Attr.u   = cur_state->REG.access;

#define GENODE_ASSERT_SELREG(REG) \
	Assert(pCtx->REG.Sel     == cur_state->REG.sel); \
	Assert(pCtx->REG.ValidSel == cur_state->REG.sel); \
	Assert(pCtx->REG.fFlags   == CPUMSELREG_FLAGS_VALID); \
	Assert(pCtx->REG.u32Limit == cur_state->REG.limit); \
	Assert(pCtx->REG.u64Base  == cur_state->REG.base);

#define GENODE_WRITE_SELREG(REG) \
	Assert(pCtx->REG.fFlags & CPUMSELREG_FLAGS_VALID); \
	Assert(pCtx->REG.ValidSel == pCtx->REG.Sel); \
	cur_state->REG.sel   = pCtx->REG.Sel; \
	cur_state->REG.limit = pCtx->REG.u32Limit; \
	cur_state->REG.base  = pCtx->REG.u64Base; \
	cur_state->REG.access = pCtx->REG.Attr.u ? : VMCS_SEG_UNUSABLE

struct Subject_state *cur_state;

Genode::Guest_interrupts *guest_interrupts;

/**
 * Return pointer to sinfo.
 */
static Genode::Sinfo * sinfo()
{
	using namespace Genode;

	static Sinfo *ptr;

	if (!ptr) {
		try {
			static Rom_connection sinfo_rom(genode_env(), "subject_info_page");
			static Sinfo sinfo(
					(addr_t)genode_env().rm().attach(sinfo_rom.dataspace()));
			ptr = &sinfo;
		} catch (...) {
			error("unable to attach Sinfo ROM");
			Assert(false);
		}
	}
	return ptr;
}


/**
 * Setup guest subject state.
 */
bool setup_subject_state()
{
	using namespace Genode;

	if (cur_state)
		return true;

	struct Sinfo::Memregion_info region;

	if (!sinfo()->get_memregion_info("monitor_state", &region)) {
		error("unable to retrieve monitor state region");
		return false;
	}

	try {
		static Attached_io_mem_dataspace subject_ds(genode_env(),
		                                            region.address,
		                                            region.size);
		cur_state = subject_ds.local_addr<struct Subject_state>();
		return true;
	} catch (...) {
		error("unable to attach subject state I/O mem dataspace");
	}
	return false;
}


/**
 * Setup guest interrupts page.
 */
bool setup_subject_interrupts()
{
	using namespace Genode;

	if (guest_interrupts)
		return true;

	struct Sinfo::Memregion_info region;

	if (!sinfo()->get_memregion_info("monitor_interrupts", &region)) {
		error("unable to retrieve monitor interrupts region");
		return false;
	}

	try {
		static Attached_io_mem_dataspace subject_intrs(genode_env(),
		                                               region.address,
		                                               region.size);
		static Guest_interrupts g((addr_t)subject_intrs.local_addr<addr_t>());
		guest_interrupts = &g;
		return true;
	} catch (...) {
		error("unable to attach subject interrupts I/O mem dataspace");
	}
	return false;
}


/**
 * Returns the value of the register identified by reg.
 * The register mapping is specified by Intel SDM Vol. 3C, table 27-3.
 */
inline uint64_t get_reg_val (struct Subject_state *cur_state, unsigned reg)
{
	switch (reg) {
		case 0:
			return cur_state->Regs.Rax;
			break;
		case 1:
			return cur_state->Regs.Rcx;
			break;
		case 2:
			return cur_state->Regs.Rdx;
			break;
		case 3:
			return cur_state->Regs.Rbx;
			break;
		case 4:
			return cur_state->Rsp;
			break;
		case 5:
			return cur_state->Regs.Rbp;
			break;
		case 6:
			return cur_state->Regs.Rsi;
			break;
		case 7:
			return cur_state->Regs.Rdi;
			break;
		default:
			Genode::error("invalid register ", reg);
			return 0;
	}
}


/**
 * Sets the control register identified by cr to the given value.
 */
inline bool set_cr(struct Subject_state *cur_state, unsigned cr, uint64_t value)
{
	bool res = false;
	switch (cr) {
		case 0:
			cur_state->Shadow_cr0 = value;
			cur_state->Cr0  = value | 1 << 5;
			cur_state->Cr0 &= ~(1 << 30 | 1 << 29);
			res = true;
			break;
		case 2:
			cur_state->Regs.Cr2 = value;
			res = true;
			break;
		case 4:
			cur_state->Shadow_cr4 = value;
			cur_state->Cr4  = value | 1 << 13;
			res = true;
			break;
		default:
			Genode::error("invalid control register ", cr);
			res = false;
	}

	return res;
}


/**
 * Handle control register access by evaluating the VM-exit qualification
 * according to Intel SDM Vol. 3C, table 27-3.
 */
inline bool handle_cr(struct Subject_state *cur_state)
{
	uint64_t qual = cur_state->Exit_qualification;
	unsigned cr  =  qual & 0xf;
	unsigned acc = (qual & 0x30) >> 4;
	unsigned reg = (qual & 0xf00) >> 8;
	bool res;

	switch (acc) {
		case 0: // MOV to CR
			res = set_cr(cur_state, cr, get_reg_val(cur_state, reg));
			break;
		default:
			Genode::error("Invalid control register ", cr, " access ", acc, ", reg ", reg);
			return false;
	}

	if (res)
		cur_state->Rip += cur_state->Instruction_len;

	return res;
}


inline void check_vm_state(PVMCPU pVCpu, struct Subject_state *cur_state)
{
	PCPUMCTX pCtx = CPUMQueryGuestCtxPtr(pVCpu);

	Assert(cur_state->Rip == pCtx->rip);
	Assert(cur_state->Rsp == pCtx->rsp);
	Assert(cur_state->Regs.Rax == pCtx->rax);
	Assert(cur_state->Regs.Rbx == pCtx->rbx);
	Assert(cur_state->Regs.Rcx == pCtx->rcx);
	Assert(cur_state->Regs.Rdx == pCtx->rdx);
	Assert(cur_state->Regs.Rbp == pCtx->rbp);
	Assert(cur_state->Regs.Rsi == pCtx->rsi);
	Assert(cur_state->Regs.Rdi == pCtx->rdi);
	Assert(cur_state->Regs.R08 == pCtx->r8);
	Assert(cur_state->Regs.R09 == pCtx->r9);
	Assert(cur_state->Regs.R10 == pCtx->r10);
	Assert(cur_state->Regs.R11 == pCtx->r11);
	Assert(cur_state->Regs.R12 == pCtx->r12);
	Assert(cur_state->Regs.R13 == pCtx->r13);
	Assert(cur_state->Regs.R14 == pCtx->r14);
	Assert(cur_state->Regs.R15 == pCtx->r15);

	Assert(cur_state->Rflags == pCtx->rflags.u);

	Assert(cur_state->Sysenter_cs  == pCtx->SysEnter.cs);
	Assert(cur_state->Sysenter_eip == pCtx->SysEnter.eip);
	Assert(cur_state->Sysenter_esp == pCtx->SysEnter.esp);

	{
		uint32_t val;
		val  = (cur_state->Shadow_cr0 & pVCpu->hm.s.vmx.u32CR0Mask);
		val |= (cur_state->Cr0 & ~pVCpu->hm.s.vmx.u32CR0Mask);
		Assert(pCtx->cr0 == val);
	}
	Assert(cur_state->Regs.Cr2 == pCtx->cr2);
	Assert(cur_state->Cr3 == pCtx->cr3);
	{
		uint32_t val;
		val  = (cur_state->Shadow_cr4 & pVCpu->hm.s.vmx.u32CR4Mask);
		val |= (cur_state->Cr4 & ~pVCpu->hm.s.vmx.u32CR4Mask);
		Assert(pCtx->cr4 == val);
	}

	GENODE_ASSERT_SELREG(cs);
	GENODE_ASSERT_SELREG(ss);
	GENODE_ASSERT_SELREG(ds);
	GENODE_ASSERT_SELREG(es);
	GENODE_ASSERT_SELREG(fs);
	GENODE_ASSERT_SELREG(gs);

	Assert(cur_state->ldtr.sel    == pCtx->ldtr.Sel);
	Assert(cur_state->ldtr.limit  == pCtx->ldtr.u32Limit);
	Assert(cur_state->ldtr.base   == pCtx->ldtr.u64Base);
	if(cur_state->ldtr.sel != 0)
		Assert(cur_state->ldtr.access == pCtx->ldtr.Attr.u);
	{
		Assert(cur_state->tr.sel    == pCtx->tr.Sel);
		Assert(cur_state->tr.limit  == pCtx->tr.u32Limit);
		Assert(cur_state->tr.base   == pCtx->tr.u64Base);
		Assert(cur_state->tr.access == pCtx->tr.Attr.u);
	}

	Assert(cur_state->idtr.limit == pCtx->idtr.cbIdt);
	Assert(cur_state->idtr.base  == pCtx->idtr.pIdt);
	Assert(cur_state->gdtr.limit == pCtx->gdtr.cbGdt);
	Assert(cur_state->gdtr.base  == pCtx->gdtr.pGdt);

	Assert(cur_state->Ia32_efer == CPUMGetGuestEFER(pVCpu));
}


inline bool has_pending_irq(PVMCPU pVCpu)
{
	return TRPMHasTrap(pVCpu) ||
		VMCPU_FF_IS_PENDING(pVCpu, (VMCPU_FF_INTERRUPT_APIC |
		                            VMCPU_FF_INTERRUPT_PIC));
}


/**
 * Return vector of currently pending IRQ.
 */
inline uint8_t get_irq(PVMCPU pVCpu)
{
	int rc;

	if (!TRPMHasTrap(pVCpu)) {
		bool res = VMCPU_FF_TEST_AND_CLEAR(pVCpu, VMCPU_FF_INTERRUPT_NMI);
		Assert(!res);

		if (VMCPU_FF_IS_PENDING(pVCpu, (VMCPU_FF_INTERRUPT_APIC |
						VMCPU_FF_INTERRUPT_PIC))) {

			uint8_t irq;
			rc = PDMGetInterrupt(pVCpu, &irq);
			Assert(RT_SUCCESS(rc));

			rc = TRPMAssertTrap(pVCpu, irq, TRPM_HARDWARE_INT);
			Assert(RT_SUCCESS(rc));
		}
	}

	Assert(TRPMHasTrap(pVCpu));

	uint8_t   u8Vector;
	TRPMEVENT enmType;
	RTGCUINT  u32ErrorCode;

	rc = TRPMQueryTrapAll(pVCpu, &u8Vector, &enmType, 0, 0, 0);
	AssertRC(rc);
	Assert(enmType == TRPM_HARDWARE_INT);
	Assert(u8Vector != X86_XCPT_NMI);

	/* Clear the pending trap. */
	rc = TRPMResetTrap(pVCpu);
	AssertRC(rc);

	return u8Vector;
}


int SUPR3QueryVTxSupported(void) { return VINF_SUCCESS; }


int SUPR3CallVMMR0Fast(PVMR0 pVMR0, unsigned uOperation, VMCPUID idCpu)
{
	static Genode::Vm_handler vm_handler(genode_env());

	switch (uOperation) {

	case SUP_VMMR0_DO_HM_RUN:
		VM     * pVM   = reinterpret_cast<VM *>(pVMR0);
		PVMCPU   pVCpu = &pVM->aCpus[idCpu];
		PCPUMCTX pCtx  = CPUMQueryGuestCtxPtr(pVCpu);
		int rc;

		Assert(!(pVCpu->cpum.s.fUseFlags & (CPUM_USED_FPU | CPUM_USED_FPU_SINCE_REM | CPUM_SYNC_FPU_STATE)));

		if (!VMCPU_FF_IS_SET(pVCpu, VMCPU_FF_INHIBIT_INTERRUPTS) && (cur_state->Intr_state & 3))
			cur_state->Intr_state &= ~3U;

		cur_state->Rip = pCtx->rip;
		cur_state->Rsp = pCtx->rsp;

		cur_state->Regs.Rax = pCtx->rax;
		cur_state->Regs.Rbx = pCtx->rbx;
		cur_state->Regs.Rcx = pCtx->rcx;
		cur_state->Regs.Rdx = pCtx->rdx;
		cur_state->Regs.Rbp = pCtx->rbp;
		cur_state->Regs.Rsi = pCtx->rsi;
		cur_state->Regs.Rdi = pCtx->rdi;
		cur_state->Regs.R08 = pCtx->r8;
		cur_state->Regs.R09 = pCtx->r9;
		cur_state->Regs.R10 = pCtx->r10;
		cur_state->Regs.R11 = pCtx->r11;
		cur_state->Regs.R12 = pCtx->r12;
		cur_state->Regs.R13 = pCtx->r13;
		cur_state->Regs.R14 = pCtx->r14;
		cur_state->Regs.R15 = pCtx->r15;

		cur_state->Rflags = pCtx->rflags.u;

		cur_state->Sysenter_cs  = pCtx->SysEnter.cs;
		cur_state->Sysenter_eip = pCtx->SysEnter.eip;
		cur_state->Sysenter_esp = pCtx->SysEnter.esp;

		set_cr(cur_state, 0, pCtx->cr0);
		set_cr(cur_state, 2, pCtx->cr2);
		set_cr(cur_state, 4, pCtx->cr4);

		GENODE_WRITE_SELREG(cs);
		GENODE_WRITE_SELREG(ss);
		GENODE_WRITE_SELREG(ds);
		GENODE_WRITE_SELREG(es);
		GENODE_WRITE_SELREG(fs);
		GENODE_WRITE_SELREG(gs);

		if (pCtx->ldtr.Sel == 0) {
			cur_state->ldtr.sel    = 0;
			cur_state->ldtr.limit  = 0;
			cur_state->ldtr.base   = 0;
			cur_state->ldtr.access = 0x82;
		} else {
			cur_state->ldtr.sel    = pCtx->ldtr.Sel;
			cur_state->ldtr.limit  = pCtx->ldtr.u32Limit;
			cur_state->ldtr.base   = pCtx->ldtr.u64Base;
			cur_state->ldtr.access = pCtx->ldtr.Attr.u;
		}
		{
			cur_state->tr.sel    = pCtx->tr.Sel;
			cur_state->tr.limit  = pCtx->tr.u32Limit;
			cur_state->tr.base   = pCtx->tr.u64Base;
			cur_state->tr.access = pCtx->tr.Attr.u;
		}

		cur_state->idtr.limit = pCtx->idtr.cbIdt;
		cur_state->idtr.base  = pCtx->idtr.pIdt;
		cur_state->gdtr.limit = pCtx->gdtr.cbGdt;
		cur_state->gdtr.base  = pCtx->gdtr.pGdt;

		cur_state->Ia32_efer = CPUMGetGuestEFER(pVCpu);

		VMCPU_SET_STATE(pVCpu, VMCPUSTATE_STARTED_EXEC);

		int irq = -1;
		if (has_pending_irq(pVCpu) &&
			cur_state->Intr_state == INTERRUPT_STATE_NONE &&
			cur_state->Rflags & X86_EFL_IF)
		{
			TRPMSaveTrap(pVCpu);
			irq = get_irq(pVCpu);
			guest_interrupts->set_pending_interrupt(irq);
		}

		vm_handler.run_vm();

		const uint64_t reason = cur_state->Exit_reason;
		uint32_t changed_state = CPUM_CHANGED_GLOBAL_TLB_FLUSH
			| CPUM_CHANGED_HIDDEN_SEL_REGS;

		switch(reason)
		{
			case VMX_EXIT_MOV_CRX:
				if (handle_cr(cur_state))
					rc = VINF_SUCCESS;
				else
					rc = VINF_EM_RAW_EMULATE_INSTR;

				break;

			case VMX_EXIT_EXT_INT:
			case VMX_EXIT_TASK_SWITCH:
			case VMX_EXIT_PREEMPT_TIMER:
				rc = VINF_SUCCESS;
				break;
			case VMX_EXIT_TRIPLE_FAULT:
				rc = VINF_EM_TRIPLE_FAULT;
				break;
			case VMX_EXIT_CPUID:
			case VMX_EXIT_HLT:
			case VMX_EXIT_INVLPG:
			case VMX_EXIT_RDTSC:
			case VMX_EXIT_MOV_DRX:
			case VMX_EXIT_IO_INSTR:
			case VMX_EXIT_RDMSR:
			case VMX_EXIT_WRMSR:
			case VMX_EXIT_PAUSE:
			case VMX_EXIT_EPT_VIOLATION:
			case VMX_EXIT_WBINVD:
				rc = VINF_EM_RAW_EMULATE_INSTR;
				break;
			default:
				rc = VINF_EM_RAW_EMULATE_INSTR;
		}

		VMCPU_SET_STATE(pVCpu, VMCPUSTATE_STARTED);

		pCtx->rip = cur_state->Rip;
		pCtx->rsp = cur_state->Rsp;

		pCtx->rax = cur_state->Regs.Rax;
		pCtx->rbx = cur_state->Regs.Rbx;
		pCtx->rcx = cur_state->Regs.Rcx;
		pCtx->rdx = cur_state->Regs.Rdx;
		pCtx->rbp = cur_state->Regs.Rbp;
		pCtx->rsi = cur_state->Regs.Rsi;
		pCtx->rdi = cur_state->Regs.Rdi;
		pCtx->r8  = cur_state->Regs.R08;
		pCtx->r9  = cur_state->Regs.R09;
		pCtx->r10 = cur_state->Regs.R10;
		pCtx->r11 = cur_state->Regs.R11;
		pCtx->r12 = cur_state->Regs.R12;
		pCtx->r13 = cur_state->Regs.R13;
		pCtx->r14 = cur_state->Regs.R14;
		pCtx->r15 = cur_state->Regs.R15;

		pCtx->rflags.u = cur_state->Rflags;

		if (pCtx->SysEnter.cs != cur_state->Sysenter_cs) {
			pCtx->SysEnter.cs = cur_state->Sysenter_cs;
			changed_state |= CPUM_CHANGED_SYSENTER_MSR;
		}
		if (pCtx->SysEnter.esp != cur_state->Sysenter_esp) {
			pCtx->SysEnter.esp = cur_state->Sysenter_esp;
			changed_state |= CPUM_CHANGED_SYSENTER_MSR;
		}
		if (pCtx->SysEnter.eip != cur_state->Sysenter_eip) {
			pCtx->SysEnter.eip = cur_state->Sysenter_eip;
			changed_state |= CPUM_CHANGED_SYSENTER_MSR;
		}

		if (pCtx->idtr.cbIdt != cur_state->idtr.limit ||
		    pCtx->idtr.pIdt  != cur_state->idtr.base)
			CPUMSetGuestIDTR(pVCpu, cur_state->idtr.base, cur_state->idtr.limit);
		if (pCtx->gdtr.cbGdt != cur_state->gdtr.limit ||
		    pCtx->gdtr.pGdt  != cur_state->gdtr.base)
			CPUMSetGuestGDTR(pVCpu, cur_state->gdtr.base, cur_state->gdtr.limit);

		{
			uint32_t val;
			val  = (cur_state->Shadow_cr0 & pVCpu->hm.s.vmx.u32CR0Mask);
			val |= (cur_state->Cr0 & ~pVCpu->hm.s.vmx.u32CR0Mask);
			if (pCtx->cr0 != val)
				CPUMSetGuestCR0(pVCpu, val);
		}
		if (pCtx->cr2 != cur_state->Regs.Cr2)
			CPUMSetGuestCR2(pVCpu, cur_state->Regs.Cr2);
		{
			uint32_t val;
			val  = (cur_state->Shadow_cr4 & pVCpu->hm.s.vmx.u32CR4Mask);
			val |= (cur_state->Cr4 & ~pVCpu->hm.s.vmx.u32CR4Mask);
			if (pCtx->cr4 != val)
				CPUMSetGuestCR4(pVCpu, val);
		}

		/*
		 * Guest CR3 must be handled after saving CR0 & CR4.
		 * See HMVMXR0.cpp, function hmR0VmxSaveGuestControlRegs
		 */
		if (pCtx->cr3 != cur_state->Cr3) {
			CPUMSetGuestCR3(pVCpu, cur_state->Cr3);
		}

		GENODE_READ_SELREG(cs);
		GENODE_READ_SELREG(ss);
		GENODE_READ_SELREG(ds);
		GENODE_READ_SELREG(es);
		GENODE_READ_SELREG(fs);
		GENODE_READ_SELREG(gs);

		if (GENODE_READ_SELREG_REQUIRED(ldtr)) {
			GENODE_READ_SELREG(ldtr);
			CPUMSetChangedFlags(pVCpu, CPUM_CHANGED_LDTR);
		}
		if (GENODE_READ_SELREG_REQUIRED(tr)) {
			GENODE_READ_SELREG(tr);
			CPUMSetChangedFlags(pVCpu, CPUM_CHANGED_TR);
		}

		CPUMSetGuestEFER(pVCpu, cur_state->Ia32_efer);

		VMCPU_FF_CLEAR(pVCpu, VMCPU_FF_TO_R3);

		CPUMSetChangedFlags(pVCpu, changed_state);

		if (cur_state->Intr_state != 0) {
			Assert(cur_state->Intr_state == BLOCKING_BY_STI ||
			       cur_state->Intr_state == BLOCKING_BY_MOV_SS);
			EMSetInhibitInterruptsPC(pVCpu, pCtx->rip);
		} else
			VMCPU_FF_CLEAR(pVCpu, VMCPU_FF_INHIBIT_INTERRUPTS);

		if ((irq != -1) && guest_interrupts->is_pending_interrupt(irq))
		{
			TRPMRestoreTrap(pVCpu);
			guest_interrupts->clear_pending_interrupt(irq);
		}

#ifdef VBOX_WITH_REM
		/* XXX see VMM/VMMR0/HMVMXR0.cpp - not necessary every time ! XXX */
		REMFlushTBs(pVM);
#endif

		return rc;
	}

	Genode::error("SUPR3CallVMMR0Fast: unhandled uOperation ", (int)uOperation);
	return VERR_INTERNAL_ERROR;
}


static Genode::Semaphore *r0_halt_sem()
{
	static Genode::Semaphore sem;
	return &sem;
}


int SUPR3CallVMMR0Ex(PVMR0 pVMR0, VMCPUID idCpu, unsigned
                     uOperation, uint64_t u64Arg, PSUPVMMR0REQHDR pReqHdr)
{
	switch(uOperation)
	{
		case VMMR0_DO_GVMM_CREATE_VM:
			{
				GVMMCREATEVMREQ &req = reinterpret_cast<GVMMCREATEVMREQ &>(*pReqHdr);
				AssertMsgReturn(req.cCpus == 1,
								("VM with multiple CPUs not supported\n"),
								VERR_INVALID_PARAMETER);
				AssertMsgReturn(setup_subject_state(),
								("Unable to map guest subject state\n"),
								VERR_INVALID_PARAMETER);
				AssertMsgReturn(setup_subject_interrupts(),
								("Unable to map guest interrupts page\n"),
								VERR_INVALID_PARAMETER);
				genode_VMMR0_DO_GVMM_CREATE_VM(pReqHdr);
				return VINF_SUCCESS;
			}

		case VMMR0_DO_GVMM_SCHED_HALT:
			r0_halt_sem()->down();
			return VINF_SUCCESS;

		case VMMR0_DO_GVMM_SCHED_WAKE_UP:
			r0_halt_sem()->up();
			return VINF_SUCCESS;

		case VMMR0_DO_VMMR0_INIT:
			{
				VM     * pVM   = reinterpret_cast<VM *>(pVMR0);
				PVMCPU   pVCpu = &pVM->aCpus[idCpu];
				pVM->hm.s.svm.fSupported = false;
				pVM->hm.s.vmx.fSupported = true;
				pVM->hm.s.vmx.fAllowUnrestricted = true;
				pVCpu->hm.s.vmx.u32CR0Mask = 0x60000020;
				pVCpu->hm.s.vmx.u32CR4Mask = 0x2000;
				return VINF_SUCCESS;
			}
		case VMMR0_DO_GVMM_SCHED_POLL:
		case VMMR0_DO_GVMM_DESTROY_VM:
		case VMMR0_DO_VMMR0_TERM:
		case VMMR0_DO_HM_SETUP_VM:
		case VMMR0_DO_HM_ENABLE:
			return VINF_SUCCESS;

		case VMMR0_DO_GVMM_SCHED_POKE:
			return VINF_SUCCESS;

		default:
			Genode::error("SUPR3CallVMMR0Ex: unhandled uOperation ", (int)uOperation);
			return VERR_GENERAL_FAILURE;
	}
}


bool create_emt_vcpu(pthread_t * thread, size_t stack_size,
                     const pthread_attr_t *attr,
                     void *(*start_routine)(void *), void *arg,
                     Genode::Cpu_session * cpu_session,
                     Genode::Affinity::Location location,
                     unsigned int cpu_id,
                     const char * name)
{
	/* No support for multiple vcpus */
	return false;
}


void genode_update_tsc(void (*update_func)(void), unsigned long update_us)
{
	using namespace Genode;

	Timer::Connection timer(genode_env());
	Signal_context    sig_ctx;
	Signal_receiver   sig_rec;
	Signal_context_capability sig_cap = sig_rec.manage(&sig_ctx);

	timer.sigh(sig_cap);
	timer.trigger_once(update_us);

	for (;;) {
		Signal s = sig_rec.wait_for_signal();
		update_func();

		timer.trigger_once(update_us);
	}
}


uint64_t genode_cpu_hz()
{
	static uint64_t cpu_freq = 0;

	if (!cpu_freq) {
		cpu_freq = sinfo()->get_tsc_khz() * 1000;
		if (!cpu_freq)
			Genode::error("unable to determine CPU frequency");
	}
	return cpu_freq;
}


HRESULT genode_setup_machine(ComObjPtr<Machine> machine)
{
	HRESULT rc;
	ULONG cCpus;
	rc = machine->COMGETTER(CPUCount)(&cCpus);
	if (FAILED(rc))
		return rc;

	if (cCpus != 1) {
		Genode::warning("configured CPUs ", cCpus, " not supported, reducing to 1.");
		rc = machine->COMSETTER(CPUCount)(1);
		if (FAILED(rc))
			return rc;
	}

	return S_OK;
}


/**
 * Dummies and unimplemented stuff.
 */


/**
 * VM memory layout on Muen is static. Always report success for revocation
 * operation.
 */
bool Vmm_memory::revoke_from_vm(Mem_region *r)
{
	return true;
}


extern "C" void pthread_yield() { Genode::warning(__func__, " unimplemented"); }
