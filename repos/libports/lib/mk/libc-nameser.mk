LIBC_NAMESER_DIR = $(LIBC_DIR)/lib/libc/nameser

SRC_C = $(notdir $(wildcard $(LIBC_NAMESER_DIR)/*.c))

include $(REP_DIR)/lib/mk/libc-common.inc

vpath %.c $(LIBC_NAMESER_DIR)

CC_CXX_WARN_STRICT =
