/*
 * \brief  Dummy implementations of symbols needed by VirtualBox
 * \author Norman Feske
 * \date   2013-08-22
 */

/*
 * Copyright (C) 2013-2014 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#include <base/log.h>

#include <string.h> /* libc memcpy */

#include "VMMInternal.h"
#include "EMInternal.h"
#include "PDMInternal.h"

#include <iprt/err.h>
#include <iprt/mem.h>
#include <VBox/vmm/cpum.h>
#include <VBox/vmm/mm.h>
#include <VBox/vmm/dbgf.h>
#include <VBox/vmm/ftm.h>
#include <VBox/vmm/selm.h>
#include <VBox/vmm/hm.h>
#include <VBox/vmm/iom.h>

#include "util.h"

static const bool trace = false;

#define TRACE(retval) \
	{ \
		if (trace) \
			Genode::log(__func__, " called, return dummy, eip=", \
			            __builtin_return_address(0)); \
		return retval; \
	}


RT_C_DECLS_BEGIN

RTDECL(int) RTMemProtect(void *pv, size_t cb, unsigned fProtect)
{
	if (!trace)
		return VINF_SUCCESS;

	char type[4];

	if (fProtect & RTMEM_PROT_READ)
		type[0] = 'r';
	else
		type[0] = '-';

	if (fProtect & RTMEM_PROT_WRITE)
		type[1] = 'w';
	else
		type[1] = '-';

	if (fProtect & RTMEM_PROT_EXEC)
		type[2] = 'x';
	else
		type[2] = '-';

	type[3] = 0;

	Genode::warning(__func__, " called - not implemented - ", pv, "+",
	                Genode::Hex(cb), " protect ", Genode::Hex(fProtect), " - "
	                "'", (char const *)type, "'");

	return VINF_SUCCESS;
}


static_assert(sizeof(RTR0PTR) == sizeof(RTR3PTR), "pointer transformation bug");
static_assert(sizeof(RTR0PTR) == sizeof(void *) , "pointer transformation bug");
static_assert(sizeof(RTR3PTR) == sizeof(RTR0PTR), "pointer transformation bug");

RTR0PTR MMHyperR3ToR0(PVM pVM, RTR3PTR R3Ptr) { return (RTR0PTR)R3Ptr; }
RTRCPTR MMHyperR3ToRC(PVM pVM, RTR3PTR R3Ptr) { return to_rtrcptr(R3Ptr); }
RTR0PTR MMHyperCCToR0(PVM pVM, void *pv)      { return (RTR0PTR)pv; }
RTRCPTR MMHyperCCToRC(PVM pVM, void *pv)      { return to_rtrcptr(pv); }
RTR3PTR MMHyperR0ToR3(PVM pVM, RTR0PTR R0Ptr) { return (RTR3PTR*)(R0Ptr | 0UL); }
RTR3PTR MMHyperRCToR3(PVM pVM, RTRCPTR RCPtr)
{
	static_assert(sizeof(RCPtr) <= sizeof(RTR3PTR), "ptr transformation bug");
	return reinterpret_cast<RTR3PTR>(0UL | RCPtr);
}

/* debugger */
int  DBGFR3AsSymbolByAddr(PUVM, RTDBGAS, PCDBGFADDRESS, uint32_t, PRTGCINTPTR,
                          PRTDBGSYMBOL, PRTDBGMOD)                              TRACE(VERR_INVALID_HANDLE)

/* called by 'VMMR3InitRC', but we don't use GC */
void CPUMPushHyper(PVMCPU, uint32_t)                                            TRACE()

int  PGMR3FinalizeMappings(PVM)                                                 TRACE(VINF_SUCCESS)
int  pgmR3InitSavedState(PVM pVM, uint64_t cbRam)                               TRACE(VINF_SUCCESS)

int  vmmR3SwitcherInit(PVM pVM)                                                 TRACE(VINF_SUCCESS)
void vmmR3SwitcherRelocate(PVM, RTGCINTPTR)                                     TRACE()
int  VMMR3DisableSwitcher(PVM)                                                  TRACE(VINF_SUCCESS)

int  emR3InitDbg(PVM pVM)                                                       TRACE(VINF_SUCCESS)

int  FTMR3Init(PVM)                                                             TRACE(VINF_SUCCESS)
int  FTMR3SetCheckpoint(PVM, FTMCHECKPOINTTYPE)                                 TRACE(-1)
int  FTMSetCheckpoint(PVM, FTMCHECKPOINTTYPE)                                   TRACE(VINF_SUCCESS)
int  FTMR3Term(PVM)                                                             TRACE(VINF_SUCCESS)

int  GIMR3Init(PVM)                                                             TRACE(VINF_SUCCESS)
void GIMR3Reset(PVM)                                                            TRACE()
bool GIMIsEnabled(PVM)                                                          TRACE(false)
bool GIMIsParavirtTscEnabled(PVM)                                               TRACE(false)
int GIMR3InitCompleted(PVM pVM)
{
	/* from original GIMR3InitCompleted code */
	if (!TMR3CpuTickIsFixedRateMonotonic(pVM, true /* fWithParavirtEnabled */))
		Genode::warning("GIM: Warning!!! Host TSC is unstable. The guest may ",
		                "behave unpredictably with a paravirtualized clock.");

	TRACE(VINF_SUCCESS)
}


void HMR3Relocate(PVM)                                                          TRACE()
void HMR3Reset(PVM pVM)                                                         TRACE()

int  SELMR3Init(PVM)                                                            TRACE(VINF_SUCCESS)
int  SELMR3Term(PVM)                                                            TRACE(VINF_SUCCESS)
int  SELMR3InitFinalize(PVM)                                                    TRACE(VINF_SUCCESS)
void SELMR3Relocate(PVM)                                                        TRACE()
void SELMR3Reset(PVM)                                                           TRACE()
void SELMR3DisableMonitoring(PVM)                                               TRACE()

int SUPR3SetVMForFastIOCtl(PVMR0)                                               TRACE(VINF_SUCCESS)

_AVLOU32NodeCore* RTAvloU32RemoveBestFit(PAVLOU32TREE, AVLOU32KEY, bool)        TRACE(VINF_SUCCESS)
int RTAvlrFileOffsetDestroy(PAVLRFOFFTREE, PAVLRFOFFCALLBACK, void*)            TRACE(VINF_SUCCESS)

/* module loader of pluggable device manager */
int  pdmR3LdrInitU(PUVM)                                                        TRACE(VINF_SUCCESS)
int  PDMR3LdrLoadVMMR0U(PUVM)                                                   TRACE(VINF_SUCCESS)
void PDMR3LdrRelocateU(PUVM, RTGCINTPTR)                                        TRACE()
int  pdmR3LoadR3U(PUVM, const char *, const char *)                             TRACE(VINF_SUCCESS)
void pdmR3LdrTermU(PUVM)                                                        TRACE()

char *pdmR3FileR3(const char * file, bool)
{
	char * pv = reinterpret_cast<char *>(RTMemTmpAllocZ(1));

	if (trace)
		Genode::log(__func__, ": file ", file, " ", (void *)pv, " ", __builtin_return_address(0));

	TRACE(pv)
}

void RTAssertMsg2Add(const char *pszFormat, ...)
{
	Genode::error(__func__, "not implemented");
	Genode::Lock lock(Genode::Lock::LOCKED);
	lock.lock();
}

RT_C_DECLS_END
