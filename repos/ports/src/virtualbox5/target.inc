VBOX_CC_OPT += -DVBOX_WITH_HARDENING
VBOX_CC_OPT += -DVBOX_WITH_GENERIC_SESSION_WATCHER

include $(REP_DIR)/lib/mk/virtualbox5-common.inc

CC_WARN += -Wall

SRC_CC = frontend/main.cc frontend/console.cc \
         frontend/VirtualBoxErrorInfoImpl.cpp \
         devices.cc drivers.cc dummies.cc libc.cc \
         mm.cc pdm.cc pgm.cc rt.cc \
         hm.cc thread.cc dynlib.cc unimpl.cc

LIBS  += base
LIBS  += stdcxx

LIBS  += virtualbox5-bios virtualbox5-recompiler virtualbox5-runtime \
         virtualbox5-vmm virtualbox5-devices virtualbox5-drivers \
         virtualbox5-storage virtualbox5-zlib virtualbox5-liblzf \
         virtualbox5-xml virtualbox5-main virtualbox5-apiwrap \
         virtualbox5-dis

LIBS  += libiconv

LIBS  += qemu-usb libyuv

INC_DIR += $(call select_from_repositories,src/lib/libc)

INC_DIR += $(VBOX_DIR)/Runtime/include

SRC_CC += HostServices/SharedFolders/service.cpp
SRC_CC += HostServices/SharedFolders/mappings.cpp
SRC_CC += HostServices/SharedFolders/vbsf.cpp
SRC_CC += HostServices/SharedFolders/vbsfpath.cpp
SRC_CC += HostServices/SharedFolders/vbsfpathabs.cpp
SRC_CC += HostServices/SharedFolders/shflhandle.cpp

SRC_CC += HostServices/SharedClipboard/service.cpp

SRC_CC += HostServices/GuestProperties/service.cpp

SRC_CC += frontend/dummy/errorinfo.cc frontend/dummy/virtualboxbase.cc
SRC_CC += frontend/dummy/autostart.cc frontend/dummy/rest.cc
SRC_CC += frontend/dummy/host.cc

INC_DIR += $(REP_DIR)/src/virtualbox5

INC_DIR += $(VBOX_DIR)/Main/include
INC_DIR += $(VBOX_DIR)/VMM/include

INC_DIR += $(REP_DIR)/src/virtualbox5/frontend
INC_DIR += $(VIRTUALBOX_DIR)/VBoxAPIWrap

INC_DIR += $(VBOX_DIR)/Main/xml
INC_DIR += $(VBOX_DIR)/HostServices

# search path to 'scan_code_set_2.h'
INC_DIR += $(call select_from_repositories,src/driver/ps2)
