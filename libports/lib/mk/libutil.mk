LIBC_UTIL_DIR = $(LIBC_DIR)/libutil

# needed by libinetutils
SRC_C = logout.c logwtmp.c trimdomain.c

INC_DIR += $(LIBC_UTIL_DIR)

include $(REP_DIR)/lib/mk/libc-common.inc

vpath %.c $(LIBC_UTIL_DIR)
