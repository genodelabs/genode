include $(REP_DIR)/lib/mk/virtualbox-common.inc

LIBS  += stdcxx

SRC_CC = pgm.cc sup.cc

INC_DIR += $(call select_from_repositories,src/lib/libc)
INC_DIR += $(VBOX_DIR)/Main/xml
INC_DIR += $(VBOX_DIR)/Main/include
INC_DIR += $(VBOX_DIR)/VMM/include
INC_DIR += $(REP_DIR)/src/virtualbox
INC_DIR += $(REP_DIR)/src/virtualbox/frontend
INC_DIR += $(REP_DIR)/src/virtualbox/accloff

vpath pgm.cc $(REP_DIR)/src/virtualbox/
vpath sup.cc $(REP_DIR)/src/virtualbox/accloff/

CC_CXX_WARN_STRICT =
