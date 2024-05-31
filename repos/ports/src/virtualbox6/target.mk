REQUIRES = x86_64

TARGET = virtualbox6

included_from_target_mk := yes
include $(REP_DIR)/lib/mk/virtualbox6-common.inc

CC_WARN += -Wall

SRC_CC := main.cc drivers.cc glx_x11.cc
SRC_CC += libc.cc unimpl.cc dummies.cc pdm.cc devices.cc nem.cc
SRC_CC += pthread.cc network.cc devxhci.cc
SRC_CC += sup.cc sup_sem.cc sup_gmm.cc sup_drv.cc sup_vm.cc sup_vcpu.cc sup_gim.cc
SRC_CC += HostServices/common/message.cpp services/services.cc

LIBS  += base
LIBS  += stdcxx
LIBS  += libiconv
LIBS  += qemu-usb libyuv
LIBS  += mesa

CC_OPT_main = -Wno-multistatement-macros
CC_OPT += -DProgress=ClientProgress

LIBS += virtualbox6-dis
LIBS += virtualbox6-sup
LIBS += virtualbox6-devices
LIBS += virtualbox6-vmm
LIBS += virtualbox6-main
LIBS += virtualbox6-xpcom
LIBS += virtualbox6-liblzf
LIBS += virtualbox6-xml
LIBS += virtualbox6-bios
LIBS += virtualbox6-zlib
LIBS += virtualbox6-storage
LIBS += virtualbox6-runtime
LIBS += virtualbox6-apiwrap
LIBS += virtualbox6-client

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
INC_DIR += $(call select_from_repositories,src/driver/ps2)

# export VirtualBox symbols to shared objects (e.g., VBoxSharedClipboard.so)
LD_OPT = --export-dynamic

LIBS += blit

vpath %.cc $(REP_DIR)/src/virtualbox6/

CC_CXX_WARN_STRICT =
