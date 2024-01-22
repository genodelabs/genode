include $(REP_DIR)/lib/mk/virtualbox6-common.inc

SRC_CC += VMM/VMMR3/VM.cpp
SRC_CC += VMM/VMMAll/VMAll.cpp
SRC_CC += VMM/VMMAll/VMMAll.cpp
SRC_CC += VMM/VMMR3/VMM.cpp

SRC_CC += VMM/VMMR3/STAM.cpp

SRC_CC += VMM/VMMR3/SSM.cpp

SRC_CC += VMM/VMMR3/NEMR3.cpp
SRC_CC += VMM/VMMAll/NEMAll.cpp

SRC_CC += VMM/VMMR3/PDM.cpp
SRC_CC += VMM/VMMR3/PDMBlkCache.cpp
SRC_CC += VMM/VMMR3/PDMDevice.cpp
SRC_CC += VMM/VMMR3/PDMQueue.cpp
SRC_CC += VMM/VMMR3/PDMCritSect.cpp
SRC_CC += VMM/VMMR3/PDMAsyncCompletion.cpp
SRC_CC += VMM/VMMR3/PDMAsyncCompletionFile.cpp
SRC_CC += VMM/VMMR3/PDMAsyncCompletionFileFailsafe.cpp
SRC_CC += VMM/VMMR3/PDMAsyncCompletionFileNormal.cpp
SRC_CC += VMM/VMMR3/PDMNetShaper.cpp
SRC_CC += VMM/VMMR3/PDMR3Task.cpp
SRC_CC += VMM/VMMAll/PDMAll.cpp
SRC_CC += VMM/VMMAll/PDMAllQueue.cpp
SRC_CC += VMM/VMMAll/PDMAllCritSect.cpp
SRC_CC += VMM/VMMAll/PDMAllCritSectRw.cpp
SRC_CC += VMM/VMMAll/PDMAllTask.cpp

SRC_CC += VMM/VMMR3/TM.cpp
SRC_CC += VMM/VMMAll/TMAll.cpp
SRC_CC += VMM/VMMAll/TMAllVirtual.cpp
SRC_CC += VMM/VMMAll/TMAllReal.cpp
SRC_CC += VMM/VMMAll/TMAllCpu.cpp
SRC_CC += VMM/VMMAll/TRPMAll.cpp

SRC_CC += VMM/VMMR3/CFGM.cpp

SRC_CC += VMM/VMMR3/PDMDevHlp.cpp

SRC_CC += VMM/VMMR3/PDMDevMiscHlp.cpp
SRC_CC += VMM/VMMR3/PDMDriver.cpp
SRC_CC += VMM/VMMR3/PDMThread.cpp

SRC_CC += VMM/VMMR3/PDMUsb.cpp

SRC_CC += VMM/VMMAll/CPUMAllMsrs.cpp
SRC_CC += VMM/VMMAll/CPUMAllRegs.cpp

SRC_CC += VMM/VMMR3/VMEmt.cpp
SRC_CC += VMM/VMMR3/VMReq.cpp

SRC_CC += VMM/VMMAll/DBGFAll.cpp
SRC_CC += VMM/VMMR3/DBGFInfo.cpp
SRC_CC += VMM/VMMR3/DBGFOS.cpp
SRC_CC += VMM/VMMR3/DBGFR3PlugIn.cpp
SRC_CC += VMM/VMMR3/DBGFR3BugCheck.cpp

SRC_CC += VMM/VMMR3/CPUM.cpp
SRC_CC += VMM/VMMR3/CPUMR3CpuId.cpp
SRC_CC += VMM/VMMR3/CPUMR3Db.cpp

SRC_CC += VMM/VMMAll/EMAll.cpp
SRC_CC += VMM/VMMR3/EM.cpp
SRC_CC += VMM/VMMR3/EMHM.cpp
SRC_CC += VMM/VMMR3/EMR3Nem.cpp

SRC_CC += VMM/VMMAll/HMAll.cpp
SRC_CC += VMM/VMMR3/HM.cpp
SRC_CC += VMM/VMMAll/HMSVMAll.cpp
SRC_CC += VMM/VMMAll/HMVMXAll.cpp

SRC_CC += VMM/VMMR3/TRPM.cpp
SRC_CC += VMM/VMMAll/SELMAll.cpp

SRC_CC += VMM/VMMR3/VMMGuruMeditation.cpp

SRC_CC += VMM/VMMAll/IEMAll.cpp
SRC_S  += VMM/VMMAll/IEMAllAImpl.asm
SRC_CC += VMM/VMMAll/IEMAllAImplC.cpp
SRC_CC += VMM/VMMR3/IEMR3.cpp

SRC_CC += VMM/VMMAll/GIMAll.cpp
SRC_CC += VMM/VMMAll/GIMAllHv.cpp
SRC_CC += VMM/VMMAll/GIMAllKvm.cpp
SRC_CC += VMM/VMMR3/GIM.cpp
SRC_CC += VMM/VMMR3/GIMHv.cpp
SRC_CC += VMM/VMMR3/GIMKvm.cpp
SRC_CC += VMM/VMMR3/GIMMinimal.cpp

SRC_CC += VMM/VMMR3/GMM.cpp

SRC_CC += VMM/VMMR3/PGM.cpp
SRC_CC += VMM/VMMR3/PGMDbg.cpp
SRC_CC += VMM/VMMR3/PGMHandler.cpp
SRC_CC += VMM/VMMR3/PGMPhys.cpp
SRC_CC += VMM/VMMR3/PGMPool.cpp
SRC_S  += VMM/VMMR3/PGMR3DbgA.asm
SRC_CC += VMM/VMMAll/PGMAll.cpp
SRC_CC += VMM/VMMAll/PGMAllHandler.cpp
SRC_CC += VMM/VMMAll/PGMAllPhys.cpp
SRC_CC += VMM/VMMAll/PGMAllPool.cpp

# C++17 does not allow the use of the 'register' specifier
CC_OPT_VMM/VMMAll/PGMAll = -Dregister=

SRC_CC += VMM/VMMR3/IOM.cpp
SRC_CC += VMM/VMMR3/IOMR3IoPort.cpp
SRC_CC += VMM/VMMR3/IOMR3Mmio.cpp
SRC_CC += VMM/VMMAll/IOMAll.cpp
SRC_CC += VMM/VMMAll/IOMAllMmioNew.cpp

SRC_CC += VMM/VMMR3/APIC.cpp
SRC_CC += VMM/VMMAll/APICAll.cpp

SRC_CC += VMM/VMMR3/MM.cpp
SRC_CC += VMM/VMMR3/MMHeap.cpp
SRC_CC += VMM/VMMR3/MMUkHeap.cpp
SRC_CC += VMM/VMMR3/MMHyper.cpp
SRC_CC += VMM/VMMR3/MMPagePool.cpp
SRC_CC += VMM/VMMAll/MMAll.cpp
SRC_CC += VMM/VMMAll/MMAllHyper.cpp

CC_OPT += -DVBOX_IN_VMM

# definitions needed by SSM.cpp
CC_OPT += -DKBUILD_TYPE=\"debug\" \
          -DKBUILD_TARGET=\"genode\" \
          -DKBUILD_TARGET_ARCH=\"x86\"

# definitions needed by VMMAll.cpp
CC_OPT += -DVBOX_SVN_REV=~0

INC_DIR += $(VBOX_DIR)/VMM/include

# override conflicting parts of the libc headers
INC_DIR += $(REP_DIR)/src/virtualbox6/include/libc

CC_CXX_WARN_STRICT =
