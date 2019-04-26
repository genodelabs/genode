LIBC_LIBKERN_DIR = $(LIBC_DIR)/sys/libkern

SRC_C = explicit_bzero.c

include $(REP_DIR)/lib/mk/libc-common.inc

vpath %.c $(LIBC_LIBKERN_DIR)

CC_CXX_WARN_STRICT =
