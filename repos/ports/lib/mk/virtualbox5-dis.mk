include $(REP_DIR)/lib/mk/virtualbox5-common.inc

SRC_CC += VMM/VMMR3/CPUMDbg.cpp
SRC_CC += VMM/VMMR3/DBGF.cpp
SRC_CC += VMM/VMMR3/DBGFAddr.cpp
SRC_CC += VMM/VMMR3/DBGFDisas.cpp
SRC_CC += VMM/VMMR3/DBGFR3Trace.cpp
SRC_CC += VMM/VMMR3/DBGFReg.cpp

SRC_CC += Disassembler/DisasmCore.cpp
SRC_CC += Disassembler/DisasmTables.cpp
SRC_CC += Disassembler/DisasmReg.cpp
SRC_CC += Disassembler/DisasmTablesX64.cpp
SRC_CC += Disassembler/DisasmFormatYasm.cpp
SRC_CC += Disassembler/DisasmFormatBytes.cpp

INC_DIR += $(VBOX_DIR)/VMM/include

CC_OPT += -DVBOX_IN_VMM
