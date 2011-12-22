LIBINC_DIR = $(REP_DIR)/lib/mk

include $(LIBINC_DIR)/kernel.inc

INC_DIR += $(REP_DIR)/src/platform
INC_DIR += $(REP_DIR)/src/core
INC_DIR += $(BASE_DIR)/src/platform

include $(LIBINC_DIR)/kernel.inc
CC_OPT += -DROOTTASK_ENTRY=_main
SRC_CC += _main.cc

vpath _main.cc $(BASE_DIR)/src/platform
