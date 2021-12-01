LIBC_GEN_RISCV_DIR = $(LIBC_DIR)/lib/libc/riscv/gen

SRC_S  = _setjmp.S setjmp.S

include $(REP_DIR)/lib/mk/libc-common.inc

vpath %.S $(LIBC_GEN_RISCV_DIR)

CC_CXX_WARN_STRICT =
