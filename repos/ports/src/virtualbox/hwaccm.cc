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
#include "HWACCMInternal.h" /* enable access to hwaccm.s.* */
#include <VBox/vmm/hwaccm.h>
#include <VBox/vmm/vm.h>

/* Genode's VirtualBox includes */
#include "sup.h"


static bool enabled = true;


VMMR3DECL(int) HWACCMR3Init(PVM pVM)
{
	/*
	 * We always set the fHWACCMEnabled flag. Otherwise, the EM won't
	 * consult us for taking scheduling decisions. The actual switch to
	 * HW accelerated mode is still dependent on the result of the
	 * HWACCMR3CanExecuteGuest function.
	 */
	pVM->fHWACCMEnabled = true;

	for (VMCPUID i = 0; i < pVM->cCpus; i++)
		pVM->aCpus[i].hwaccm.s.fActive = false;

	return VINF_SUCCESS;
}


VMMR3_INT_DECL(int) HWACCMR3InitCompleted(PVM pVM, VMINITCOMPLETED enmWhat)
{
	enabled = pVM->hwaccm.s.svm.fSupported || pVM->hwaccm.s.vmx.fSupported;

	if (!enabled || enmWhat != VMINITCOMPLETED_RING0)
		return VINF_SUCCESS;

	int rc = SUPR3CallVMMR0Ex(pVM->pVMR0, 0 /*idCpu*/, VMMR0_DO_HWACC_SETUP_VM, 0, NULL);
	return rc;
}


VMMR3DECL(bool) HWACCMR3IsVmxPreemptionTimerUsed(PVM pVM)
{
	PLOG("HWACCMR3IsVmxPreemptionTimerUsed");
	return false;
}


VMMR3DECL(bool) HWACCMR3IsActive(PVMCPU pVCpu)
{
	return pVCpu->hwaccm.s.fActive;
}


VMMR3DECL(bool) HWACCMR3IsRescheduleRequired(PVM pVM, PCPUMCTX pCtx)
{
	/* no re-schedule on AMD-V required - just works */
/*
	if (pVM->hwaccm.s.svm.fSupported)
		return false;
*/
	bool reschedule = !CPUMIsGuestInPagedProtectedModeEx(pCtx);

//	PLOG("reschedule %u %u %lx", reschedule, HWACCMR3CanExecuteGuest(pVM, pCtx), pCtx->cr0);

	return reschedule;
}


void HWACCMR3PagingModeChanged(PVM pVM, PVMCPU pVCpu, PGMMODE enmShadowMode,
                               PGMMODE enmGuestMode)
{
//	PLOG("HWACCMR3PagingModeChanged: enmShadowMode=%d enmGuestMode=%d",
//	     enmShadowMode, enmGuestMode);
}


VMMR3DECL(bool) HWACCMR3IsEventPending(PVMCPU pVCpu)
{
//	PLOG("HWACCMR3IsEventPending false");
	return false;
}


VMMR3DECL(bool) HWACCMR3CanExecuteGuest(PVM pVM, PCPUMCTX pCtx)
{
	PVMCPU pVCpu = VMMGetCpu(pVM);

	/* AMD-V just works */
/*
	if (pVM->hwaccm.s.svm.fSupported) {
		pVCpu->hwaccm.s.fActive = true;
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
	pVCpu->hwaccm.s.fActive = res;

	return res;
}
