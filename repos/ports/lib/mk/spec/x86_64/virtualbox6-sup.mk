include $(REP_DIR)/lib/mk/virtualbox6-common.inc

LIBS  += stdcxx

SRC_CC := HostDrivers/Support/SUPLib.cpp
SRC_CC += HostDrivers/Support/SUPLibLdr.cpp

INC_DIR += $(VIRTUALBOX_DIR)/VBoxAPIWrap
INC_DIR += $(VBOX_DIR)/HostDrivers/Support
INC_DIR += $(VBOX_DIR)/Devices/Bus

INC_DIR += $(VBOX_DIR)/Main/xml
INC_DIR += $(VBOX_DIR)/Main/include
INC_DIR += $(VBOX_DIR)/VMM/include

CC_CXX_WARN_STRICT =
