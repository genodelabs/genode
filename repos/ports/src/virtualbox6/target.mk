REQUIRES = x86_64

TARGET = virtualbox6

include $(REP_DIR)/lib/mk/virtualbox6-common.inc

CC_WARN += -Wall

SRC_CC := main.cc drivers.cc
SRC_CC += libc.cc unimpl.cc dummies.cc pdm.cc devices.cc nem.cc dynlib.cc
SRC_CC += pthread.cc network.cc devxhci.cc
SRC_CC += sup.cc sup_sem.cc sup_gmm.cc sup_drv.cc sup_vm.cc sup_vcpu.cc sup_gim.cc
SRC_CC += HostServices/common/message.cpp

LIBS  += base
LIBS  += stdcxx
LIBS  += libiconv
LIBS  += qemu-usb

CC_OPT_main = -Wno-multistatement-macros
CC_OPT += -DProgress=ClientProgress

LIB_MK_FILES := $(notdir $(wildcard $(REP_DIR)/lib/mk/virtualbox6-*.mk) \
                         $(wildcard $(REP_DIR)/lib/mk/spec/x86_64/virtualbox6-*.mk))

LIBS += $(LIB_MK_FILES:.mk=)

INC_DIR += $(call select_from_repositories,src/lib/libc)
INC_DIR += $(call select_from_repositories,src/lib/libc)/spec/x86_64

INC_DIR += $(REP_DIR)/src/virtualbox6
INC_DIR += $(VBOX_DIR)/HostDrivers/Support
INC_DIR += $(VBOX_DIR)/Main/include
INC_DIR += $(VBOX_DIR)/Main/src-server
INC_DIR += $(VBOX_DIR)/Main/xml
INC_DIR += $(VBOX_DIR)/NetworkServices
INC_DIR += $(VBOX_DIR)/Runtime/include
INC_DIR += $(VBOX_DIR)/VMM/include
INC_DIR += $(VIRTUALBOX_DIR)/VBoxAPIWrap
INC_DIR += $(VIRTUALBOX_DIR)/include/VBox/Graphics

# search path to 'scan_code_set_1.h'
INC_DIR += $(call select_from_repositories,src/drivers/ps2)

LIBS += blit

vpath %.cc $(REP_DIR)/src/virtualbox6/

CC_CXX_WARN_STRICT =
