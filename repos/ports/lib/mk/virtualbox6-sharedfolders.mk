include $(REP_DIR)/lib/mk/virtualbox6-common.inc

SHARED_LIB = yes

SRC_CC += mappings.cpp
SRC_CC += shflhandle.cpp
SRC_CC += VBoxSharedFoldersSvc.cpp
SRC_CC += vbsf.cpp
SRC_CC += vbsfpathabs.cpp
SRC_CC += vbsfpath.cpp

LIBS += stdcxx

INC_DIR += $(VBOX_DIR)/HostServices/SharedFolders

CC_CXX_WARN_STRICT =

vpath %.cpp $(VBOX_DIR)/HostServices/SharedFolders
