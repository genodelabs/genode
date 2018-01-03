LIBC_ISC_DIR = $(LIBC_DIR)/lib/libc/isc

INC_DIR += $(LIBC_DIR)/lib/libc/isc

SRC_C = $(notdir $(wildcard $(LIBC_ISC_DIR)/*.c))

include $(REP_DIR)/lib/mk/libc-common.inc

vpath %.c $(LIBC_ISC_DIR)

CC_CXX_WARN_STRICT =
