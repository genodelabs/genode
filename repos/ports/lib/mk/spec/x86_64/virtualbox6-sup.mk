include $(REP_DIR)/lib/mk/virtualbox6-common.inc

LIBS  += stdcxx

SRC_CC := sup.cc sup_sem.cc sup_gmm.cc sup_drv.cc sup_vm.cc vcpu.cc

SRC_CC += HostDrivers/Support/SUPLib.cpp
SRC_CC += HostDrivers/Support/SUPLibLdr.cpp

INC_DIR += $(call select_from_repositories,src/lib/libc)

INC_DIR += $(VIRTUALBOX_DIR)/VBoxAPIWrap
INC_DIR += $(VBOX_DIR)/HostDrivers/Support
INC_DIR += $(VBOX_DIR)/Devices/Bus
INC_DIR += $(REP_DIR)/src/virtualbox6

INC_DIR += $(VBOX_DIR)/Main/xml
INC_DIR += $(VBOX_DIR)/Main/include
INC_DIR += $(VBOX_DIR)/VMM/include

vpath %.cc $(REP_DIR)/src/virtualbox6

CC_CXX_WARN_STRICT =
