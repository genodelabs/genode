LIBC_GEN_ARM64_DIR = $(LIBC_DIR)/lib/libc/aarch64/gen

SRC_S  = _setjmp.S setjmp.S

include $(REP_DIR)/lib/mk/libc-common.inc

vpath %.S $(LIBC_GEN_ARM64_DIR)

