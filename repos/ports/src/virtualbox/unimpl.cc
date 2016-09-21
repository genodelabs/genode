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

#include <base/log.h>

#include <iprt/assert.h>

extern "C" {

#define DUMMY(name) \
void name(void) { \
	Genode::warning(__func__, ": " #name " called, not implemented, eip=", \
	                __builtin_return_address(0)); \
	while (1) { Assert(!"not implemented"); } \
}

DUMMY(DBGFR3CoreWrite)
DUMMY(DBGCRegisterCommands)
DUMMY(DBGFR3EventAssertion)
DUMMY(DBGFR3EventBreakpoint)
DUMMY(DBGFR3EventSrc)
DUMMY(DBGFR3VMMForcedAction)
DUMMY(DBGFR3DisasInstrEx)
DUMMY(DBGFR3DisasInstrCurrentLogInternal)
DUMMY(DBGFR3StackWalkBegin)
DUMMY(DBGFR3StackWalkBeginEx)
DUMMY(DBGFR3StackWalkNext)
DUMMY(DBGFR3StackWalkEnd)

DUMMY(HMInvalidatePage)
DUMMY(HMR3EmulateIoBlock)
DUMMY(HMR3PatchTprInstr)
DUMMY(HMR3CheckError)
DUMMY(HMR3RestartPendingIOInstr)
DUMMY(HMR3EnablePatching)
DUMMY(HMR3DisablePatching)
DUMMY(HMGetPaePdpes)
DUMMY(HMSetSingleInstruction)

DUMMY(IEMExecOne)
DUMMY(IEMExecLots)

DUMMY(MMHyperR0ToCC)
DUMMY(MMHyperRCToCC)

DUMMY(MMR3HeapAPrintfV)
DUMMY(MMR3LockCall)
DUMMY(PDMR3AsyncCompletionTemplateCreateDriver)
DUMMY(PDMR3LdrGetInterfaceSymbols)
DUMMY(PDMR3LdrQueryRCModFromPC)
DUMMY(PDMCritSectBothFF)

DUMMY(PGMPhysGCPtr2GCPhys)
DUMMY(PGMPhysSimpleReadGCPhys)
DUMMY(PGMPhysSimpleReadGCPtr)
DUMMY(PGMPhysSimpleWriteGCPtr)
DUMMY(PGMSyncCR3)

DUMMY(PGMR3SharedModuleCheckAll)
DUMMY(PGMR3SharedModuleUnregister)
DUMMY(PGMR3SharedModuleRegister)
DUMMY(PGMR3MappingsUnfix)
DUMMY(PGMR3MappingsFix)
DUMMY(PGMR3MappingsDisable)
DUMMY(PGMR3LockCall)
DUMMY(PGMR3PoolGrow)
DUMMY(PGMR3QueryGlobalMemoryStats)
DUMMY(PGMR3QueryMemoryStats)

DUMMY(PGMR3PhysAllocateHandyPages)
DUMMY(PGMR3PhysAllocateLargeHandyPage)
DUMMY(PGMR3PhysChangeMemBalloon)
DUMMY(PGMR3PhysChunkMap)
DUMMY(PGMR3PhysGCPhys2CCPtrExternal)
DUMMY(PGMR3PhysGCPhys2CCPtrReadOnlyExternal)
DUMMY(PGMR3PhysMMIO2MapKernel)
DUMMY(PGMR3PhysReadU16)
DUMMY(PGMR3PhysRomProtect)

DUMMY(PGMPrefetchPage)
DUMMY(PGMGstGetPage)
DUMMY(PGMGstIsPagePresent)
DUMMY(PGMGstUpdatePaePdpes)
DUMMY(PGMShwMakePageReadonly)
DUMMY(PGMShwMakePageNotPresent)
DUMMY(PGMPhysIsGCPhysNormal)
DUMMY(PGMHandlerVirtualChangeInvalidateCallback)
DUMMY(PGMSetLargePageUsage)
DUMMY(PGMPhysSimpleDirtyWriteGCPtr)
DUMMY(PGMGetShadowMode)
DUMMY(PGMGetHostMode)

DUMMY(PGMUpdateCR3)
DUMMY(PGMGetGuestMode)

DUMMY(RTPoll)
DUMMY(RTPollSetAdd)
DUMMY(RTPollSetCreate)
DUMMY(RTPollSetEventsChange)
DUMMY(RTPollSetRemove)
DUMMY(RTPollSetDestroy)

DUMMY(RTProcCreate)
DUMMY(RTProcTerminate)
DUMMY(RTProcWait)
DUMMY(RTLdrGetSuff)

DUMMY(RTPathAppend)
DUMMY(RTPathChangeToDosSlashes)
DUMMY(RTSemEventWaitEx)

DUMMY(RTMemDupExTag)
DUMMY(RTMemDupTag)
DUMMY(RTMemExecFree)

DUMMY(SELMR3GetSelectorInfo)
DUMMY(SELMR3GetShadowSelectorInfo)

DUMMY(SUPR3HardenedLdrLoadPlugIn)

DUMMY(SUPSemEventMultiWaitNoResume)
DUMMY(SUPSemEventMultiReset)

DUMMY(VMMR3GetHostToGuestSwitcher)

DUMMY(RTHeapSimpleRelocate)
DUMMY(RTHeapSimpleAlloc)
DUMMY(RTHeapSimpleInit)
DUMMY(RTHeapSimpleFree)
DUMMY(RTAvloU32Remove)
DUMMY(RTAvloU32Get)
DUMMY(RTAvloU32GetBestFit)
DUMMY(RTAvlU32Destroy)
DUMMY(RTAvlU32GetBestFit)
DUMMY(RTAvloU32DoWithAll)
DUMMY(RTAvloU32Insert)
DUMMY(RTAvlU32Get)
DUMMY(RTAvlU32DoWithAll)
DUMMY(RTAvlU32Insert)

DUMMY(IOMInterpretOUT)
DUMMY(IOMInterpretOUTS)
DUMMY(IOMInterpretIN)
DUMMY(IOMInterpretINS)

DUMMY(DISInstrToStrWithReader)
DUMMY(DISInstrToStrEx)
DUMMY(DISFetchRegSegEx)

DUMMY(RTFileQueryFsSizes)

DUMMY(RTAvlrFileOffsetGet)
DUMMY(RTAvlrFileOffsetGetBestFit)
DUMMY(RTAvlrFileOffsetInsert)
DUMMY(RTAvlrFileOffsetRemove)
DUMMY(RTAvlrU64Destroy)
DUMMY(RTAvlrU64DoWithAll)
DUMMY(RTAvlrU64GetBestFit)
DUMMY(RTAvlrU64Insert)
DUMMY(RTAvlrU64RangeGet)
DUMMY(RTAvlrU64RangeRemove)
DUMMY(RTAvlrU64Remove)
DUMMY(RTSocketToNative)

DUMMY(RTStrCat)
DUMMY(RTStrCatP)
DUMMY(RTStrStr)

DUMMY(RTTcpClientCloseEx)
DUMMY(RTTcpClientConnectEx)
DUMMY(RTTcpFlush)
DUMMY(RTTcpGetLocalAddress)
DUMMY(RTTcpGetPeerAddress)
DUMMY(RTTcpRead)
DUMMY(RTTcpReadNB)
DUMMY(RTTcpSelectOne)
DUMMY(RTTcpSelectOneEx)
DUMMY(RTTcpSetSendCoalescing)
DUMMY(RTTcpSgWrite)
DUMMY(RTTcpSgWriteNB)
DUMMY(RTTcpWrite)
DUMMY(RTTcpWriteNB)
DUMMY(RTTimeLocalExplode)

DUMMY(RTSymlinkCreate)
DUMMY(RTSymlinkRead)
DUMMY(RTSymlinkDelete)

DUMMY(RTNetIPv6PseudoChecksumEx)

DUMMY(pthread_mutex_timedlock)
DUMMY(pthread_kill)

DUMMY(RTZipXarFsStreamFromIoStream)

DUMMY(FTMR3CancelStandby)
DUMMY(FTMR3PowerOn)

} /* extern "C" */
