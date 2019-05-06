include $(REP_DIR)/lib/mk/virtualbox5-common.inc

LIBS  += stdcxx

SRC_CC = sup.cc sup_vmm.cc

INC_DIR += $(call select_from_repositories,src/lib/libc)

INC_DIR += $(VIRTUALBOX_DIR)/VBoxAPIWrap

INC_DIR += $(VBOX_DIR)/Main/xml
INC_DIR += $(VBOX_DIR)/Main/include
INC_DIR += $(VBOX_DIR)/VMM/include
INC_DIR += $(REP_DIR)/src/virtualbox5
INC_DIR += $(REP_DIR)/src/virtualbox5/frontend

vpath sup_vmm.cc $(REP_DIR)/src/virtualbox5/generic
vpath sup.cc     $(REP_DIR)/src/virtualbox5/generic

CC_CXX_WARN_STRICT =
