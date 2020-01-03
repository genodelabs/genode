LIBC_RESOLV_DIR = $(LIBC_DIR)/lib/libc/resolv

FILTER_OUT = res_debug.c

SRC_C = $(filter-out $(FILTER_OUT),$(notdir $(wildcard $(LIBC_RESOLV_DIR)/*.c)))

include $(REP_DIR)/lib/mk/libc-common.inc

INC_DIR += sys/sys
INC_DIR += $(LIBC_DIR)/sys/sys

CC_DEF += -DSTDERR_FILENO=2

# suppress "warning: ‘strncpy’ specified bound depends on the length of the source argument"
CC_OPT_res_query := -Wno-stringop-overflow

vpath %.c $(LIBC_RESOLV_DIR)

CC_CXX_WARN_STRICT =
