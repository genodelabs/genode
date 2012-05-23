LIBC_GEN_I386_DIR = $(LIBC_DIR)/libc/i386/gen

SRC_S  = _setjmp.S setjmp.S

include $(REP_DIR)/lib/mk/libc-common.inc

vpath %.S $(LIBC_GEN_I386_DIR)
