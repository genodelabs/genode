include $(REP_DIR)/lib/mk/virtualbox6-common.inc

SRC_CC += VMM/VMMR3/CPUMDbg.cpp
SRC_CC += VMM/VMMR3/DBGF.cpp
SRC_CC += VMM/VMMR3/DBGFAddr.cpp
SRC_CC += VMM/VMMR3/DBGFDisas.cpp
SRC_CC += VMM/VMMR3/DBGFMem.cpp
SRC_CC += VMM/VMMR3/DBGFR3Trace.cpp
SRC_CC += VMM/VMMR3/DBGFReg.cpp

SRC_CC += $(addprefix Disassembler/, $(notdir $(wildcard $(VBOX_DIR)/Disassembler/*.cpp)))

INC_DIR += $(VBOX_DIR)/VMM/include

CC_OPT += -DVBOX_IN_VMM

CC_CXX_WARN_STRICT =
