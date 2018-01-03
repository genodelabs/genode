LIBC_LOCALE_DIR = $(LIBC_DIR)/lib/libc/locale

SRC_C = $(notdir $(wildcard $(LIBC_LOCALE_DIR)/*.c))

include $(REP_DIR)/lib/mk/libc-common.inc

vpath %.c $(LIBC_LOCALE_DIR)

CC_CXX_WARN_STRICT =
