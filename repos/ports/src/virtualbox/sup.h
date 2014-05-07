/*
 * \brief  Common VirtualBox SUPLib supplements
 * \author Norman Feske
 * \date   2013-08-20
 */

/*
 * Copyright (C) 2013 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#ifndef _SUP_H_
#define _SUP_H_

/* Genode includes */
#include "util/misc_math.h"
#include "util/string.h"

/* VirtualBox includes */
#include <VBox/vmm/vm.h>
#include <VBox/vmm/gvmm.h>
#include <iprt/param.h>

/* libc memory allocator */
#include <libc_mem_alloc.h>

uint64_t genode_cpu_hz();

void inline genode_VMMR0_DO_GVMM_CREATE_VM(PSUPVMMR0REQHDR pReqHdr)
{
	GVMMCREATEVMREQ &req = reinterpret_cast<GVMMCREATEVMREQ &>(*pReqHdr);

	size_t const cCpus = req.cCpus;

	/*
	 * Allocate and initialize VM struct
	 *
	 * The VM struct is followed by the variable-sizedA array of VMCPU
	 * objects. 'RT_UOFFSETOF' is used to determine the size including
	 * the VMCPU array.
	 *
	 * VM struct must be page-aligned, which is checked at least in
	 * PDMR3CritSectGetNop().
	 */
	size_t const cbVM = RT_UOFFSETOF(VM, aCpus[cCpus]);
	VM *pVM = (VM *)Libc::mem_alloc()->alloc(cbVM, Genode::log2(PAGE_SIZE));
	Genode::memset(pVM, 0, cbVM);

	/*
	 * On Genode, VMMR0 and VMMR3 share a single address space. Hence, the
	 * same pVM pointer is valid as pVMR0 and pVMR3.
	 */
	pVM->enmVMState       = VMSTATE_CREATING;
	pVM->pVMR0            = (RTHCUINTPTR)pVM;
	pVM->pVMRC            = (RTGCUINTPTR)pVM;
	pVM->pSession         = req.pSession;
	pVM->cbSelf           = cbVM;
	pVM->cCpus            = cCpus;
	pVM->uCpuExecutionCap = 100;  /* expected by 'vmR3CreateU()' */
	pVM->offVMCPU         = RT_UOFFSETOF(VM, aCpus);

	for (uint32_t i = 0; i < cCpus; i++) {
		pVM->aCpus[i].pVMR0           = pVM->pVMR0;
		pVM->aCpus[i].pVMR3           = pVM;
		pVM->aCpus[i].idHostCpu       = NIL_RTCPUID;
		pVM->aCpus[i].hNativeThreadR0 = NIL_RTNATIVETHREAD;
	}

	/* out parameters of the request */
	req.pVMR0 = pVM->pVMR0;
	req.pVMR3 = pVM;
}

#endif /* _SUP_H_ */
