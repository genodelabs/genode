LIBC_GEN_AMD64_DIR = $(LIBC_DIR)/libc/amd64/gen

SRC_S  = _setjmp.S setjmp.S

include $(REP_DIR)/lib/mk/libc-common.inc

vpath %.S $(LIBC_GEN_AMD64_DIR)
