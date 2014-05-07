include $(REP_DIR)/lib/mk/seoul_libc_support.mk

SRC_C += fenv.c

vpath fenv.c $(LIBC_DIR)/msun/amd64

include $(REP_DIR)/lib/mk/virtualbox_libc_support.inc
