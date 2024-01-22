/*
 * \brief  Dummy implementations of symbols needed by VirtualBox
 * \author Norman Feske
 * \author Christian Helmuth
 * \date   2013-08-22
 */

/*
 * Copyright (C) 2013-2021 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#include <iprt/assert.h>

extern "C" {

#define DUMMY(name) \
void name(void) { \
	Genode::error(__func__, ": " #name " called, not implemented, eip=", \
	              __builtin_return_address(0)); \
	for (;;); \
}

DUMMY(DBGFR3CoreWrite)
DUMMY(DBGFR3LogModifyDestinations)
DUMMY(DBGFR3LogModifyFlags)
DUMMY(DBGFR3LogModifyGroups)
DUMMY(DBGFR3StackWalkBegin)
DUMMY(DBGFR3StackWalkBeginEx)
DUMMY(DBGFR3StackWalkEnd)
DUMMY(DBGFR3StackWalkNext)
DUMMY(drvHostBaseDestructOs)
DUMMY(drvHostBaseDoLockOs)
DUMMY(drvHostBaseEjectOs)
DUMMY(drvHostBaseFlushOs)
DUMMY(drvHostBaseGetMediaSizeOs)
DUMMY(drvHostBaseInitOs)
DUMMY(drvHostBaseIsMediaPollingRequiredOs)
DUMMY(drvHostBaseMediaRefreshOs)
DUMMY(drvHostBaseOpenOs)
DUMMY(drvHostBaseQueryMediaStatusOs)
DUMMY(drvHostBaseReadOs)
DUMMY(drvHostBaseScsiCmdGetBufLimitOs)
DUMMY(drvHostBaseScsiCmdOs)
DUMMY(drvHostBaseWriteOs)
DUMMY(PDMCritSectBothFF)
DUMMY(PDMNsAllocateBandwidth)
DUMMY(PDMR3LdrEnumModules)
DUMMY(PDMR3LdrGetInterfaceSymbols)
DUMMY(PGMR3MappingsFix)
DUMMY(PGMR3MappingsUnfix)
DUMMY(PGMR3SharedModuleCheckAll)
DUMMY(PGMR3SharedModuleRegister)
DUMMY(PGMR3SharedModuleUnregister)
DUMMY(RTCrStoreCertAddFromDir)
DUMMY(RTCrStoreCertAddFromFile)
DUMMY(RTCrStoreCreateInMem)
DUMMY(RTDbgAsLineByAddr)
DUMMY(RTDbgAsLockExcl)
DUMMY(RTDbgAsModuleByIndex)
DUMMY(RTDbgAsModuleCount)
DUMMY(RTDbgAsModuleLink)
DUMMY(RTDbgAsModuleQueryMapByIndex)
DUMMY(RTDbgAsModuleUnlink)
DUMMY(RTDbgAsSymbolByAddr)
DUMMY(RTDbgAsUnlockExcl)
DUMMY(RTDbgCfgRelease)
DUMMY(RTDbgLineDup)
DUMMY(RTDbgLineFree)
DUMMY(RTDbgModCreateFromImage)
DUMMY(RTDbgModLineByAddr)
DUMMY(RTDbgModName)
DUMMY(RTDbgModRelease)
DUMMY(RTDbgModSegmentRva)
DUMMY(RTDbgModSymbolByAddr)
DUMMY(RTDbgModUnwindFrame)
DUMMY(RTDbgSymbolDup)
DUMMY(RTDbgSymbolFree)
DUMMY(RTFileQueryFsSizes)
DUMMY(RTFsIsoMakerCmdEx)
DUMMY(RTProcCreate)
DUMMY(RTProcCreateEx)
DUMMY(RTZipXarFsStreamFromIoStream)
DUMMY(SELMR3GetSelectorInfo)
DUMMY(SUPGetCpuHzFromGipForAsyncMode)
DUMMY(SUPReadTscWithDelta)
DUMMY(USBFilterClone)
DUMMY(VDIfTcpNetInstDefaultDestroy)

/* xpcom */
DUMMY(_MD_CreateUnixProcess)
DUMMY(_MD_CreateUnixProcessDetached)
DUMMY(_MD_DetachUnixProcess)
DUMMY(_MD_KillUnixProcess)
DUMMY(_MD_WaitUnixProcess)
DUMMY(PR_FindSymbol)
DUMMY(PR_GetIdentitiesLayer)
DUMMY(PR_LoadLibrary)
DUMMY(PR_LoadLibraryWithFlags)
DUMMY(PR_UnloadLibrary)
DUMMY(_PR_CleanupLayerCache)
DUMMY(_PR_MakeNativeIPCName)
DUMMY(_PR_MapOptionName)
DUMMY(_PR_ShutdownLinker)

} /* extern "C" */
