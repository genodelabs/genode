include $(REP_DIR)/lib/mk/virtualbox-common.inc

#
# Prevent inclusion of the Genode::Log definition after the vbox #define
# of 'Log'. Otherwise, the attemt to compile base/log.h will fail.
#
VBOX_CC_OPT += -include base/log.h

LIBS  += stdcxx

SRC_CC = sup.cc pgm.cc

INC_DIR += $(call select_from_repositories,src/lib/libc)
INC_DIR += $(call select_from_repositories,src/lib/pthread)

INC_DIR += $(VBOX_DIR)/Main/xml
INC_DIR += $(VBOX_DIR)/Main/include
INC_DIR += $(VBOX_DIR)/VMM/include
INC_DIR += $(REP_DIR)/src/virtualbox
INC_DIR += $(REP_DIR)/src/virtualbox/frontend
INC_DIR += $(REP_DIR)/src/virtualbox/spec/nova

vpath pgm.cc $(REP_DIR)/src/virtualbox/
vpath sup.cc $(REP_DIR)/src/virtualbox/spec/nova/
