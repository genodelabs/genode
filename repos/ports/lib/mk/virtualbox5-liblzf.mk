include $(REP_DIR)/lib/mk/virtualbox5-common.inc

LIBLZF_DIR = $(VIRTUALBOX_DIR)/src/libs/liblzf-3.4
INC_DIR   += $(LIBLZF_DIR)
CC_OPT    += -DULTRA_FAST=1 -DHLOG=12 -DSTRICT_ALIGN=0 -DPIC
SRC_C      = lzf_c.c lzf_d.c

vpath % $(LIBLZF_DIR)

CC_CXX_WARN_STRICT =
