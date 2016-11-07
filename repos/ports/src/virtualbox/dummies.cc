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

#include <util/string.h>
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
#include <VBox/vmm/iem.h>
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

RTDECL(int) RTMemProtect(void *pv, size_t cb, unsigned fProtect) RT_NO_THROW
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
	                "'", Genode::Cstring(type), "'");

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
int  DBGFR3Init(PVM)                                                            TRACE(VINF_SUCCESS)
int  DBGFR3EventSrcV(PVM, DBGFEVENTTYPE, const char *, unsigned, const char *,
                    const char *, va_list)                                      TRACE(VINF_SUCCESS)
void DBGFR3Relocate(PVM, RTGCINTPTR)                                            TRACE()
int  DBGFR3RegRegisterDevice(PVM, PCDBGFREGDESC, PPDMDEVINS, const char*,
                             uint32_t)                                          TRACE(VINF_SUCCESS)
int  DBGFR3AsSymbolByAddr(PUVM, RTDBGAS, PCDBGFADDRESS, uint32_t, PRTGCINTPTR,
                          PRTDBGSYMBOL, PRTDBGMOD)                              TRACE(VERR_INVALID_HANDLE)
int DBGFR3Term(PVM)                                                             TRACE(VINF_SUCCESS)
int DBGFR3Event(PVM pVM, DBGFEVENTTYPE enmEvent)
{
	Genode::log(__func__, ": ", (int)enmEvent);

	TRACE(VERR_NOT_SUPPORTED)
}


/* called by 'VMMR3InitRC', but we don't use GC */
int  cpumR3DbgInit(PVM)                                                         TRACE(VINF_SUCCESS)
void CPUMPushHyper(PVMCPU, uint32_t)                                            TRACE()

int  PGMFlushTLB(PVMCPU, uint64_t, bool)                                        TRACE(VINF_SUCCESS) 
int  PGMInvalidatePage(PVMCPU, RTGCPTR)                                         TRACE(VINF_SUCCESS)
int  PGMHandlerPhysicalPageTempOff(PVM, RTGCPHYS, RTGCPHYS)                     TRACE(VINF_SUCCESS)
void PGMPhysReleasePageMappingLock(PVM, PPGMPAGEMAPLOCK)                        TRACE()
int  PGMR3CheckIntegrity(PVM)                                                   TRACE(VINF_SUCCESS)
int  PGMR3FinalizeMappings(PVM)                                                 TRACE(VINF_SUCCESS)
int  PGMR3InitCompleted(PVM, VMINITCOMPLETED)                                   TRACE(VINF_SUCCESS)
int  PGMR3InitDynMap(PVM)                                                       TRACE(VINF_SUCCESS)
int  PGMR3InitFinalize(PVM)                                                     TRACE(VINF_SUCCESS)
int  PGMR3HandlerVirtualRegister(PVM, PGMVIRTHANDLERTYPE, RTGCPTR, RTGCPTR,
                                 PFNPGMR3VIRTINVALIDATE, PFNPGMR3VIRTHANDLER,
                                 const char*, const char*, const char*)         TRACE(VINF_SUCCESS)
int  PGMHandlerVirtualDeregister(PVM, RTGCPTR)                                  TRACE(VINF_SUCCESS)
void PGMR3Relocate(PVM, RTGCINTPTR)                                             TRACE()
int  PGMChangeMode(PVMCPU, uint64_t, uint64_t, uint64_t)                        TRACE(VINF_SUCCESS)
int  PGMR3ChangeMode(PVM, PVMCPU, PGMMODE)                                      TRACE(VINF_SUCCESS)
/* required for Netware */
void PGMCr0WpEnabled(PVMCPU pVCpu)                                              TRACE()

/* debugger */
void DBGFR3PowerOff(PVM pVM) TRACE()
int  DBGFR3DisasInstrCurrent(PVMCPU, char *, uint32_t)                          TRACE(VINF_SUCCESS)

int  vmmR3SwitcherInit(PVM pVM)                                                 TRACE(VINF_SUCCESS)
void vmmR3SwitcherRelocate(PVM, RTGCINTPTR)                                     TRACE()
int  VMMR3DisableSwitcher(PVM)                                                  TRACE(VINF_SUCCESS)

int  emR3InitDbg(PVM pVM)                                                       TRACE(VINF_SUCCESS)

int  FTMR3Init(PVM)                                                             TRACE(VINF_SUCCESS)
int  FTMR3SetCheckpoint(PVM, FTMCHECKPOINTTYPE)                                 TRACE(-1)
int  FTMSetCheckpoint(PVM, FTMCHECKPOINTTYPE)                                   TRACE(VINF_SUCCESS)
int  FTMR3Term(PVM)                                                             TRACE(VINF_SUCCESS)

int  IEMR3Init(PVM)                                                             TRACE(VINF_SUCCESS)
int  IEMR3Term(PVM)                                                             TRACE(VINF_SUCCESS)
void IEMR3Relocate(PVM)                                                         TRACE()

void HMR3Relocate(PVM)                                                          TRACE()
void HMR3Reset(PVM pVM)                                                         TRACE()

int  SELMR3Init(PVM)                                                            TRACE(VINF_SUCCESS)
int  SELMR3Term(PVM)                                                            TRACE(VINF_SUCCESS)
int  SELMR3InitFinalize(PVM)                                                    TRACE(VINF_SUCCESS)
void SELMR3Relocate(PVM)                                                        TRACE()
void SELMR3Reset(PVM)                                                           TRACE()
void SELMR3DisableMonitoring(PVM)                                               TRACE()

int IOMR3IOPortRegisterRC(PVM, PPDMDEVINS, RTIOPORT, RTUINT, RTRCPTR, RTRCPTR,
                          RTRCPTR, RTRCPTR, RTRCPTR, const char*)               TRACE(VINF_SUCCESS)
int IOMR3IOPortRegisterR0(PVM, PPDMDEVINS, RTIOPORT, RTUINT, RTR0PTR,
                          RTHCUINTPTR, RTHCUINTPTR, RTHCUINTPTR, RTHCUINTPTR,
                          const char*)                                          TRACE(VINF_SUCCESS)
int IOMR3MmioRegisterR0(PVM, PPDMDEVINS, RTGCPHYS, uint32_t, RTR0PTR,
                        RTHCUINTPTR, RTHCUINTPTR, RTHCUINTPTR)                  TRACE(VINF_SUCCESS)
int IOMR3MmioRegisterRC(PVM, PPDMDEVINS, RTGCPHYS, uint32_t, RTGCPTR, RTRCPTR,
                        RTRCPTR, RTRCPTR)                                       TRACE(VINF_SUCCESS)
void IOMR3Relocate(PVM, RTGCINTPTR)                                             TRACE()
void IOMR3Reset(PVM)                                                            TRACE()

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

RT_C_DECLS_END
