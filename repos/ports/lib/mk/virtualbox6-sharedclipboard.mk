include $(REP_DIR)/lib/mk/virtualbox6-common.inc

SHARED_LIB = yes

SRC_CC += sharedclipboard.cc
SRC_CC += message.cpp
SRC_CC += VBoxSharedClipboardSvc.cpp
SRC_CC += clipboard-common.cpp

LIBS += stdcxx

INC_DIR += $(VBOX_DIR)/HostServices/SharedClipboard

CC_CXX_WARN_STRICT =

vpath sharedclipboard.cc         $(REP_DIR)/src/virtualbox6/services
vpath message.cpp                $(VBOX_DIR)/HostServices/common
vpath VBoxSharedClipboardSvc.cpp $(VBOX_DIR)/HostServices/SharedClipboard
vpath clipboard-common.cpp       $(VBOX_DIR)/GuestHost/SharedClipboard
