LIBC_STDIO_DIR = $(LIBC_DIR)/lib/libc/stdio

SRC_C = $(notdir $(wildcard $(LIBC_STDIO_DIR)/*.c))

include $(REP_DIR)/lib/mk/libc-common.inc

vpath %.c $(LIBC_STDIO_DIR)

CC_CXX_WARN_STRICT =
