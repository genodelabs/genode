REQUIRES += 64bit

SHARED_LIB = yes

include $(REP_DIR)/lib/import/import-solo5.mk

INC_DIR += $(SOLO5_PORT_DIR)/src/lib/solo5/bindings
INC_DIR += $(REP_DIR)/src/lib/solo5

CC_OPT += -D__SOLO5_BINDINGS__ -Drestrict=__restrict__

SRC_CC = bindings.cc

vpath %.cc $(SOLO5_PORT_DIR)/src/lib/solo5/bindings/genode
