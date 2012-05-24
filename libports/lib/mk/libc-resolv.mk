LIBC_RESOLV_DIR = $(LIBC_DIR)/libc/resolv

SRC_C = $(notdir $(wildcard $(LIBC_RESOLV_DIR)/*.c))

include $(REP_DIR)/lib/mk/libc-common.inc

vpath %.c $(LIBC_RESOLV_DIR)
