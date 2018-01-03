LIBS       = libc

include $(REP_DIR)/lib/mk/mesa-common.inc

SRC_C    = drivers/dri/swrast/swrast.c
INC_DIR += $(MESA_SRC_DIR)/drivers/dri/common

vpath %.c $(MESA_SRC_DIR)

CC_CXX_WARN_STRICT =
