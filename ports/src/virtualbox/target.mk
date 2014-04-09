VBOX_CC_OPT += -DVBOX_WITH_HARDENING

include $(REP_DIR)/lib/mk/virtualbox-common.inc

TARGET = virtualbox
SRC_CC = main.cc cxx_dummies.cc devices.cc drivers.cc dummies.cc libc.cc \
         logger.cc mm.cc pdm.cc pgm.cc rt.cc sup.cc iommio.cc ioport.cc \
         hwaccm.cc thread.cc 

LIBS  += base
LIBS  += config_args

LIBS  += virtualbox-bios virtualbox-recompiler virtualbox-runtime \
         virtualbox-vmm virtualbox-devices virtualbox-drivers \
         virtualbox-storage virtualbox-zlib virtualbox-liblzf \
         virtualbox-hwaccl virtualbox-dis

INC_DIR += $(call select_from_repositories,src/lib/libc)
INC_DIR += $(call select_from_repositories,src/lib/pthread)

INC_DIR += $(VBOX_DIR)/Runtime/include
INC_DIR += $(VBOX_DIR)/Frontends/VBoxBFE

SRC_CC += Frontends/VBoxBFE/VBoxBFE.cpp
SRC_CC += Frontends/VBoxBFE/DisplayImpl.cpp
SRC_CC += Frontends/VBoxBFE/VMMDevInterface.cpp
SRC_CC += Frontends/VBoxBFE/KeyboardImpl.cpp
SRC_CC += Frontends/VBoxBFE/MachineDebuggerImpl.cpp
SRC_CC += Frontends/VBoxBFE/StatusImpl.cpp

INC_DIR += $(VBOX_DIR)/Main/include
INC_DIR += $(VBOX_DIR)/VMM/include

# search path to 'scan_code_set_2.h'
INC_DIR += $(call select_from_repositories,src/drivers/input/ps2)
