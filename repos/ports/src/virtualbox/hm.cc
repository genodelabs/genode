/*
 * \brief  VirtualBox hardware-acceleration manager
 * \author Norman Feske
 * \date   2013-08-20
 */

/*
 * Copyright (C) 2013 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

/* Genode includes */
#include <base/printf.h>

/* VirtualBox includes */
#include "HMInternal.h" /* enable access to hm.s.* */
#include <VBox/vmm/hm.h>
#include <VBox/vmm/vm.h>

/* Genode's VirtualBox includes */
#include "sup.h"


static bool enabled = true;


VMMR3DECL(int) HMR3Init(PVM pVM)
{
	/*
	 * We always set the fHMEnabled flag. Otherwise, the EM won't
	 * consult us for taking scheduling decisions. The actual switch to
	 * HW accelerated mode is still dependent on the result of the
	 * HMR3CanExecuteGuest function.
	 */
	pVM->fHMEnabled = true;

	for (VMCPUID i = 0; i < pVM->cCpus; i++)
		pVM->aCpus[i].hm.s.fActive = false;

	pVM->fHMEnabledFixed = true;

	return VINF_SUCCESS;
}


VMMR3_INT_DECL(int) HMR3Term(PVM pVM)
{
	return VINF_SUCCESS;
}


VMMR3_INT_DECL(int) HMR3InitCompleted(PVM pVM, VMINITCOMPLETED enmWhat)
{
	enabled = pVM->hm.s.svm.fSupported || pVM->hm.s.vmx.fSupported;

	if (!enabled || enmWhat != VMINITCOMPLETED_RING0)
		return VINF_SUCCESS;

	int rc = SUPR3CallVMMR0Ex(pVM->pVMR0, 0 /*idCpu*/, VMMR0_DO_HM_SETUP_VM, 0, NULL);

	if (rc == VINF_SUCCESS) {
		CPUMSetGuestCpuIdFeature(pVM, CPUMCPUIDFEATURE_SEP);
	}

	return rc;
}


VMMDECL(bool) HMIsEnabledNotMacro(PVM pVM)
{
	Assert(pVM->fHMEnabledFixed);
	return pVM->fHMEnabled;
}


VMMR3DECL(bool) HMR3IsVmxPreemptionTimerUsed(PVM pVM)
{
//	PLOG("HMR3IsVmxPreemptionTimerUsed");
	return false;
}


VMMR3DECL(bool) HMR3IsActive(PVMCPU pVCpu)
{
	return pVCpu->hm.s.fActive;
}

VMM_INT_DECL(bool) HMIsLongModeAllowed(PVM pVM)                                 
{                                                                               
	return HMIsEnabled(pVM) && pVM->hm.s.fAllow64BitGuests;                     
} 

VMMR3DECL(bool) HMR3IsRescheduleRequired(PVM pVM, PCPUMCTX pCtx)
{
	/* no re-schedule on AMD-V required - just works */
/*
	if (pVM->hm.s.svm.fSupported)
		return false;
*/
	bool reschedule = !CPUMIsGuestInPagedProtectedModeEx(pCtx);

//	PLOG("reschedule %u %u %lx", reschedule, HMR3CanExecuteGuest(pVM, pCtx), pCtx->cr0);

	return reschedule;
}


VMMR3DECL(bool) HMR3IsEventPending(PVMCPU pVCpu)
{
//	PLOG("HMR3IsEventPending false");
	return false;
}


VMMR3DECL(bool) HMR3CanExecuteGuest(PVM pVM, PCPUMCTX pCtx)
{
	PVMCPU pVCpu = VMMGetCpu(pVM);

	/* AMD-V just works */
/*
	if (pVM->hm.s.svm.fSupported) {
		pVCpu->hm.s.fActive = true;
		return true;
	}
*/
	if (!enabled)
		return false;

	/* enable H/W acceleration in protected mode only */
	bool res = (pCtx->cr0 & 1) && (pCtx->cr0 & 0x80000000);
/*
	static bool on = false;

	if (res)
		on = true;

	if (on)
		PLOG("executeguest %lx -> %x", pCtx->cr0, res);
*/
	pVCpu->hm.s.fActive = res;

	return res;
}
