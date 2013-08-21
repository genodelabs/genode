/*
 * \brief  Dummy implementations of symbols needed by VirtualBox
 * \author Norman Feske
 * \date   2013-08-22
 */

/*
 * Copyright (C) 2013 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#include <base/printf.h>
#include <base/thread.h>

#include <stddef.h>
#include <time.h>

extern "C" {

	typedef long DUMMY;

#define DUMMY(retval, name) \
DUMMY name(void) { \
	PDBG( #name " called, not implemented, eip=%p", __builtin_return_address(0)); \
	for (;;); \
	return retval; \
}

#define CHECKED_DUMMY(retval, name) \
DUMMY name(void) { \
	PINF( #name " called, not implemented, eip=%p", __builtin_return_address(0)); \
	return retval; \
}

CHECKED_DUMMY( 0, cpumR3DbgInit)
CHECKED_DUMMY( 0, DBGFR3Init)  /* debugger */
DUMMY(-1, DBGFR3CoreWrite)
CHECKED_DUMMY( 0, FTMR3Init)  /* fault tolerance manager */
CHECKED_DUMMY( 0, pdmR3LdrInitU) /* module loader of pluggable device manager */
CHECKED_DUMMY( 0, PDMR3LdrLoadVMMR0U) /* pretend to have successfully loaded the r0 module */
CHECKED_DUMMY( 0, pdmR3LoadR3U)
CHECKED_DUMMY( 0, pthread_atfork)
CHECKED_DUMMY( 0, pthread_attr_setdetachstate)
CHECKED_DUMMY( 0, pthread_attr_setstacksize)
CHECKED_DUMMY( 0, RTMemProtect)
CHECKED_DUMMY( 0, SELMR3Init)  /* selector manager - GDT handling */
CHECKED_DUMMY( 0, sigfillset)
CHECKED_DUMMY( 0, vmmR3SwitcherInit)  /* world switcher */
CHECKED_DUMMY(-1, atexit)
CHECKED_DUMMY(-1, getpid)
CHECKED_DUMMY(-1, pdmR3FileR3)
CHECKED_DUMMY(-1, setlocale)
CHECKED_DUMMY(-1, sigaddset)
CHECKED_DUMMY(-1, sigemptyset)
CHECKED_DUMMY(-1, siginterrupt)
CHECKED_DUMMY(-1, sysctl)
DUMMY( 0, RTErrCOMGet)
void CPUMPushHyper() { } /* called by 'VMMR3InitRC', but we don't use GC */
DUMMY(-1, DBGCRegisterCommands)
DUMMY(-1, DBGFR3Event)
DUMMY(-1, DBGFR3EventAssertion)
DUMMY(-1, DBGFR3EventBreakpoint)
DUMMY(-1, DBGFR3EventSrc)
CHECKED_DUMMY( 0, DBGFR3EventSrcV)
void DBGFR3Relocate() { }
DUMMY(-1, DBGFR3Term)
DUMMY(-1, DBGFR3VMMForcedAction)

CHECKED_DUMMY(-4, DBGFR3AsSymbolByAddr) /* -4 == VERR_INVALID_HANDLE */

DUMMY(-1, _flockfile)

int FTMR3SetCheckpoint() { return -1; }
DUMMY(-1, FTMR3Term)
int FTMSetCheckpoint() { return 0; }
DUMMY(-1, _funlockfile)
DUMMY(-1, _fwalk)

DUMMY(-1, HWACCMInvalidatePage)
DUMMY(-1, HWACCMFlushTLB)
DUMMY(-1, HWACCMR3EmulateIoBlock)
DUMMY(-1, HWACCMR3PatchTprInstr)
DUMMY(-1, HWACCMR3CheckError)
DUMMY(-1, HWACCMR3RestartPendingIOInstr)
void HWACCMR3Relocate() { }
DUMMY(-1, HWACCMR3Reset)
DUMMY(-1, HWACCMR3Term)
DUMMY(-1, HWACMMR3EnablePatching)
DUMMY(-1, HWACMMR3DisablePatching)

CHECKED_DUMMY( 0, IEMR3Init)  /* interpreted execution manager (seems to be just a skeleton) */
void IEMR3Relocate() { }
DUMMY(-1, IEMR3Term)

DUMMY(-1, MMHyperR0ToCC)
DUMMY(-1, MMHyperR0ToR3)
DUMMY(-1, MMHyperRCToCC)
DUMMY(-1, MMHyperRCToR3)
CHECKED_DUMMY(0, MMHyperGetArea)

DUMMY(-1, MMR3HeapAPrintfV)
CHECKED_DUMMY( 0, MMR3HyperInitFinalize)
CHECKED_DUMMY( 0, MMR3HyperSetGuard)
DUMMY(-1, MMR3LockCall)
DUMMY(-1, MMR3Term)
DUMMY(-1, MMR3TermUVM)
DUMMY(-1, PDMR3AsyncCompletionTemplateCreateDriver)
DUMMY(-1, PDMR3LdrGetInterfaceSymbols)
CHECKED_DUMMY( 0, PDMR3LdrRelocateU)
DUMMY(-1, pdmR3LdrTermU)

DUMMY(-1, PGMNotifyNxeChanged)
DUMMY(-1, PGMPhysGCPtr2GCPhys)
DUMMY(-1, PGMPhysSimpleReadGCPhys)
DUMMY(-1, PGMPhysSimpleReadGCPtr)
DUMMY(-1, PGMPhysSimpleWriteGCPtr)
DUMMY(-1, PGMSyncCR3)

CHECKED_DUMMY( 0, PGMR3CheckIntegrity)
CHECKED_DUMMY( 0, PGMR3FinalizeMappings)
CHECKED_DUMMY( 0, PGMR3InitCompleted)
CHECKED_DUMMY( 0, PGMR3InitDynMap)  /* reserve space for "dynamic mappings" */
CHECKED_DUMMY( 0, PGMR3InitFinalize)

DUMMY(-1, PGMR3SharedModuleCheckAll)
DUMMY(-1, PGMR3SharedModuleUnregister)
DUMMY(-1, PGMR3SharedModuleRegister)
DUMMY(-1, PGMR3MappingsSize)
DUMMY(-1, PGMR3MappingsUnfix)
DUMMY(-1, PGMR3PhysChangeMemBalloon)
DUMMY(-1, PGMR3MappingsFix)
CHECKED_DUMMY( 0, PGMR3MappingsDisable)
DUMMY(-1, PGMR3LockCall)
DUMMY(-1, PGMR3PhysAllocateHandyPages)
DUMMY(-1, PGMR3PhysAllocateLargeHandyPage)
DUMMY(-1, PGMR3PhysChunkMap)
DUMMY(-1, PGMR3PhysGCPhys2CCPtrExternal)
DUMMY(-1, PGMR3PhysGCPhys2CCPtrReadOnlyExternal)
DUMMY(-1, PGMR3PhysMMIO2Deregister)
DUMMY(-1, PGMR3PhysMMIO2MapKernel)
DUMMY(-1, PGMR3PhysReadU16)
DUMMY(-1, PGMR3PhysReadU64)
DUMMY(-1, PGMR3PhysRomProtect)
DUMMY(-1, PGMR3PoolGrow)
void PGMR3Relocate() {}
DUMMY(-1, PGMR3ResetCpu)
DUMMY(-1, PGMR3Term)

DUMMY(-1, PGMPrefetchPage)
DUMMY(-1, PGMGstGetPage)
DUMMY(-1, PGMGstIsPagePresent)
DUMMY(-1, PGMShwMakePageReadonly)
DUMMY(-1, PGMShwMakePageNotPresent)
DUMMY(-1, PGMPhysIsGCPhysNormal)
DUMMY(-1, PGMHandlerVirtualChangeInvalidateCallback)
DUMMY(-1, PGMSetLargePageUsage)
DUMMY(-1, PGMPhysSimpleDirtyWriteGCPtr)
DUMMY(-1, PGMGetShadowMode)
DUMMY(-1, PGMGetHostMode)

CHECKED_DUMMY(0, poll)  /* needed by 'DrvHostSerial.cpp' */
DUMMY(-1, printf)
DUMMY(-1, pthread_key_delete)
DUMMY(-1, reallocf)
DUMMY(-1, RTCrc32);
DUMMY(-1, RTCrc32Start)
DUMMY(-1, RTCrc32Finish)
DUMMY(-1, RTCrc32Process)
DUMMY(-1, RTMemExecFree)
DUMMY(-1, RTMemPageFree)
DUMMY(-1, RTPathHasPath)
DUMMY(-1, RTPathAppend)
DUMMY(-1, rtPathPosixRename)
CHECKED_DUMMY(0, rtProcInitExePath)
DUMMY(-1, RTSemEventWaitEx)

CHECKED_DUMMY( 0, SELMR3InitFinalize)
void SELMR3Relocate() { }
CHECKED_DUMMY( 0, SELMR3DisableMonitoring)
DUMMY(-1, SELMR3Reset)
DUMMY(-1, SELMR3Term)
DUMMY(-1, SELMR3GetSelectorInfo)

DUMMY(-1, libc_select_notify) /* needed for libc_terminal plugin */
DUMMY(-1, strdup)
DUMMY(-1, DISInstrToStrEx)
CHECKED_DUMMY(-1, signal)

DUMMY(-1, strcat)
DUMMY(-1, strerror)
DUMMY(-1, strpbrk)

CHECKED_DUMMY( 0, SUPR3SetVMForFastIOCtl)
DUMMY(-1, SUPR3HardenedLdrLoadPlugIn)
DUMMY(-1, SUPR3Term)

CHECKED_DUMMY(100000*10, SUPSemEventMultiGetResolution) /* called by 'vmR3HaltGlobal1Init' */
CHECKED_DUMMY(-1, __swsetup)

DUMMY(-1, VMMR3FatalDump)
void vmmR3SwitcherRelocate() { }
CHECKED_DUMMY( 0, VMMR3DisableSwitcher)
DUMMY(-1, VMMR3GetHostToGuestSwitcher)

DUMMY(-1, pthread_kill)
DUMMY(-1, sscanf)
DUMMY(-1, RTHeapSimpleRelocate)
DUMMY(-1, RTHeapOffsetInit)
DUMMY(-1, RTHeapSimpleInit)
DUMMY(-1, RTHeapOffsetFree)
DUMMY(-1, RTHeapSimpleFree)
DUMMY(-1, RTAvloU32Get)
DUMMY(-1, RTAvloU32Remove)
DUMMY(-1, RTAvloU32GetBestFit)
CHECKED_DUMMY(0, RTAvloU32RemoveBestFit)
DUMMY(-1, RTAvlU32Destroy)
DUMMY(-1, RTAvlU32GetBestFit)
DUMMY(-1, RTAvloU32DoWithAll)
DUMMY(-1, RTAvloU32Insert)
DUMMY(-1, RTAvlU32Get)
DUMMY(-1, RTAvlU32DoWithAll)
DUMMY(-1, RTAvlU32Insert)

CHECKED_DUMMY( 0, IOMR3Init)
int IOMR3IOPortRegisterR0() { return 0; }
int IOMR3IOPortRegisterRC() { return 0; }
DUMMY(-1, IOMR3MmioDeregister)
CHECKED_DUMMY( 0, IOMR3MmioRegisterR0)
CHECKED_DUMMY( 0, IOMR3MmioRegisterRC)
void IOMR3Relocate() { }
DUMMY(-1, IOMR3Reset)
DUMMY(-1, IOMR3Term)

DUMMY(-1, IOMInterpretOUT)
DUMMY(-1, IOMInterpretOUTS)
DUMMY(-1, IOMInterpretIN)
DUMMY(-1, IOMInterpretINS)


DUMMY(-1, DISInstrToStrWithReader)

DUMMY(0, RTPathQueryInfoEx)

DUMMY(-1, RTFileQueryFsSizes)

time_t mktime(tm *) {
	PERR("mktime not implemented, return 0");
	return 0;
}

DUMMY(-1, pthread_mutex_timedlock)

CHECKED_DUMMY( 0, PGMHandlerVirtualDeregister) /* XXX */
CHECKED_DUMMY( 0, PGMR3HandlerVirtualRegister) /* XXX */

/*
 * Dummies added for storage
 */
DUMMY(-1, closedir)
DUMMY(-1, readdir_r)
DUMMY(-1, RTAvlrFileOffsetDestroy)
DUMMY(-1, RTAvlrFileOffsetGet)
DUMMY(-1, RTAvlrFileOffsetGetBestFit)
DUMMY(-1, RTAvlrFileOffsetInsert)
DUMMY(-1, RTAvlrFileOffsetRemove)
DUMMY(-1, RTAvlrU64Destroy)
DUMMY(-1, RTAvlrU64DoWithAll)
DUMMY(-1, RTAvlrU64GetBestFit)
DUMMY(-1, RTAvlrU64Insert)
DUMMY(-1, RTAvlrU64RangeGet)
DUMMY(-1, RTAvlrU64RangeRemove)
DUMMY(-1, RTAvlrU64Remove)
DUMMY(-1, RTDirOpenFiltered)
DUMMY(-1, RTDirReadEx)
DUMMY(-1, RTDirClose)
DUMMY(-1, RTLdrClose)
DUMMY(-1, RTLdrGetSymbol)
DUMMY(-1, RTMemDupExTag)
DUMMY(-1, RTPathQueryInfo)
DUMMY(-1, rtPathRootSpecLen)
DUMMY(-1, RTPathStartsWithRoot)
DUMMY(-1, RTSocketToNative)
DUMMY(-1, RTStrCatP)
DUMMY(-1, RTTcpClientCloseEx)
DUMMY(-1, RTTcpClientConnect)
DUMMY(-1, RTTcpFlush)
DUMMY(-1, RTTcpGetLocalAddress)
DUMMY(-1, RTTcpGetPeerAddress)
DUMMY(-1, RTTcpRead)
DUMMY(-1, RTTcpReadNB)
DUMMY(-1, RTTcpSelectOne)
DUMMY(-1, RTTcpSelectOneEx)
DUMMY(-1, RTTcpSetSendCoalescing)
DUMMY(-1, RTTcpSgWrite)
DUMMY(-1, RTTcpSgWriteNB)
DUMMY(-1, RTTcpWrite)
DUMMY(-1, RTTcpWriteNB)
DUMMY(-1, strncat)

int __isthreaded;

int sigprocmask() { return 0; }
int _sigprocmask() { return 0; }

int  PGMFlushTLB() { return 0; }
int PGMInvalidatePage() { return 0; }  /* seems to be needed on raw mode only */
int  PGMHandlerPhysicalPageTempOff() { return 0; }

int  PGMIsLockOwner() { return 0; }  /* assertion in EMRemLock */
bool IOMIsLockOwner() { return 0; }  /* XXX */

int  MMHyperIsInsideArea() { return 0; } /* used by dbgfR3DisasInstrRead */
int  PGMPhysReleasePageMappingLock() { return 0; }
} /* extern "C" */

