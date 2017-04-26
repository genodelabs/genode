VBOX_CC_OPT += -DVBOX_WITH_HARDENING
VBOX_CC_OPT += -DVBOX_WITH_GENERIC_SESSION_WATCHER

include $(REP_DIR)/lib/mk/virtualbox-common.inc

#
# Prevent inclusion of the Genode::Log definition after the vbox #define
# of 'Log'. Otherwise, the attemt to compile base/log.h will fail.
#
VBOX_CC_OPT += -include base/log.h

CC_WARN += -Wall

SRC_CC = frontend/main.cc frontend/console.cc \
         frontend/VBoxAPIWrap/MediumFormatWrap.cpp \
         frontend/VBoxAPIWrap/TokenWrap.cpp \
         frontend/VirtualBoxErrorInfoImpl.cpp \
         devices.cc drivers.cc dummies.cc libc.cc \
         logger.cc mm.cc pdm.cc rt.cc sup.cc iommio.cc ioport.cc \
         hm.cc thread.cc dynlib.cc unimpl.cc

LIBS  += base
LIBS  += stdcxx

LIBS  += virtualbox-bios virtualbox-recompiler virtualbox-runtime \
         virtualbox-vmm virtualbox-devices virtualbox-drivers \
         virtualbox-storage virtualbox-zlib virtualbox-liblzf \
         virtualbox-xml virtualbox-main

LIBS  += pthread libc_terminal libc_pipe libiconv

LIBS  += qemu-usb

INC_DIR += $(call select_from_repositories,src/lib/libc)
INC_DIR += $(call select_from_repositories,src/lib/pthread)

INC_DIR += $(VBOX_DIR)/Runtime/include

SRC_CC += HostServices/SharedFolders/service.cpp
SRC_CC += HostServices/SharedFolders/mappings.cpp
SRC_CC += HostServices/SharedFolders/vbsf.cpp
SRC_CC += HostServices/SharedFolders/shflhandle.cpp

SRC_CC += HostServices/SharedClipboard/service.cpp

SRC_CC += HostServices/GuestProperties/service.cpp

SRC_CC += frontend/dummy/errorinfo.cc frontend/dummy/virtualboxbase.cc
SRC_CC += frontend/dummy/autostart.cc frontend/dummy/rest.cc
SRC_CC += frontend/dummy/host.cc

INC_DIR += $(VBOX_DIR)/Main/include
INC_DIR += $(VBOX_DIR)/VMM/include

INC_DIR += $(REP_DIR)/src/virtualbox/frontend
INC_DIR += $(REP_DIR)/src/virtualbox/frontend/VBoxAPIWrap

INC_DIR += $(VBOX_DIR)/Main/xml
INC_DIR += $(VBOX_DIR)/HostServices

# search path to 'scan_code_set_2.h'
INC_DIR += $(call select_from_repositories,src/drivers/input/spec/ps2)
