LIBC_NAMESER_DIR = $(LIBC_DIR)/libc/nameser

SRC_C = $(notdir $(wildcard $(LIBC_NAMESER_DIR)/*.c))

include $(REP_DIR)/lib/mk/libc-common.inc

vpath %.c $(LIBC_NAMESER_DIR)
