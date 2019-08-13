/*
 * \brief  Dummy implementations of symbols needed by VirtualBox
 * \author Norman Feske
 * \date   2013-08-22
 */

/*
 * Copyright (C) 2013-2017 Genode Labs GmbH
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

DUMMY(DBGCRegisterCommands)

DUMMY(DBGFR3AsLineByAddrA)
DUMMY(DBGFR3AsSymbolByAddrA)
DUMMY(DBGFR3CoreWrite)
DUMMY(DBGFR3LogModifyDestinations)
DUMMY(DBGFR3LogModifyFlags)
DUMMY(DBGFR3LogModifyGroups)
DUMMY(DBGFR3OSDetect)
DUMMY(DBGFR3OSQueryNameAndVersion)
DUMMY(DBGFR3OSQueryInterface)
DUMMY(DBGFR3PlugInLoad)
DUMMY(DBGFR3PlugInLoadAll)
DUMMY(DBGFR3StackWalkBegin)
DUMMY(DBGFR3StackWalkBeginEx)
DUMMY(DBGFR3StackWalkNext)
DUMMY(DBGFR3StackWalkEnd)
DUMMY(DBGFR3PagingDumpEx)
DUMMY(DBGFR3PlugInUnload)
DUMMY(DBGFR3PlugInUnloadAll)

DUMMY(HBDMgrDestroy)

DUMMY(HMR3CheckError)
DUMMY(HMR3DisablePatching)
DUMMY(HMR3EnablePatching)
DUMMY(HMR3EmulateIoBlock)
DUMMY(HMR3IsEnabled)
DUMMY(HMR3IsNestedPagingActive)
DUMMY(HMR3IsUXActive)
DUMMY(HMR3IsVpidActive)
DUMMY(HMR3PatchTprInstr)

DUMMY(MMHyperR0ToCC)
DUMMY(MMHyperRCToCC)

DUMMY(MMR3HyperRealloc)
DUMMY(MMR3LockCall)
DUMMY(MMR3PageDummyHCPhys)
DUMMY(MMR3UkHeapFree)

DUMMY(PDMR3AsyncCompletionBwMgrSetMaxForFile)
DUMMY(PDMR3LdrGetInterfaceSymbols)
DUMMY(PDMR3LdrQueryRCModFromPC)
DUMMY(PDMCritSectBothFF)

DUMMY(pgmMapResolveConflicts)
DUMMY(pgmR3SyncPTResolveConflict)
DUMMY(pgmR3SyncPTResolveConflictPAE)

DUMMY(PGMR3HandlerVirtualRegister)

DUMMY(MMPagePhys2PageEx)
DUMMY(PGMR3DbgReadGCPtr)
DUMMY(PGMR3DbgR3Ptr2GCPhys)

DUMMY(PGMR3MappingsUnfix)
DUMMY(PGMR3MappingsFix)
DUMMY(PGMR3MappingsDisable)

DUMMY(PGMR3SharedModuleCheckAll)
DUMMY(PGMR3SharedModuleUnregister)
DUMMY(PGMR3SharedModuleRegister)

DUMMY(pgmR3MapInfo)

DUMMY(RTTraceBufCarve)
DUMMY(RTTraceBufEnumEntries)
DUMMY(RTTraceBufGetEntryCount)
DUMMY(RTTraceBufGetEntrySize)

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
DUMMY(RTLdrLoadAppPriv)

DUMMY(RTPathChangeToDosSlashes)
DUMMY(RTSemEventWaitEx)
DUMMY(RTSemPing)
DUMMY(RTSemPingWait)

DUMMY(RTMemExecFree)

DUMMY(RTMpGetPresentCount)

DUMMY(SELMR3GetSelectorInfo)
DUMMY(SELMR3GetShadowSelectorInfo)

DUMMY(SUPReadTscWithDelta)
DUMMY(SUPR3ContAlloc)
DUMMY(SUPR3ContFree)
DUMMY(SUPR3HardenedLdrLoadPlugIn)
DUMMY(SUPR3PageAlloc)
DUMMY(SUPR3PageFree)
DUMMY(SUPR3PageMapKernel)
DUMMY(SUPR3ReadTsc)
DUMMY(SUPGetCpuHzFromGipForAsyncMode)

DUMMY(VMMR3GetHostToGuestSwitcher)

DUMMY(RTHeapSimpleRelocate)
DUMMY(RTHeapSimpleAlloc)
DUMMY(RTHeapSimpleInit)
DUMMY(RTHeapSimpleFree)
DUMMY(RTAvloU32Remove)
DUMMY(RTAvloU32Get)
DUMMY(RTAvloU32GetBestFit)
DUMMY(RTAvloU32DoWithAll)
DUMMY(RTAvloU32Insert)

DUMMY(IOMInterpretOUT)
DUMMY(IOMInterpretOUTS)
DUMMY(IOMInterpretIN)
DUMMY(IOMInterpretINS)

DUMMY(DISInstrToStrWithReader)
DUMMY(DISInstrToStrEx)

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

DUMMY(RTSystemQueryAvailableRam)

DUMMY(RTNetIPv6PseudoChecksumEx)

DUMMY(pthread_mutex_timedlock)
DUMMY(pthread_kill)

DUMMY(RTZipXarFsStreamFromIoStream)

DUMMY(FTMR3CancelStandby)
DUMMY(FTMR3PowerOn)

DUMMY(GIMExecHypercallInstr)
DUMMY(GIMReadMsr)
DUMMY(GIMWriteMsr)

DUMMY(RTDbgLineFree)
DUMMY(RTDbgSymbolFree)
} /* extern "C" */
