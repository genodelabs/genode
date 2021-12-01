include $(REP_DIR)/lib/mk/libc.mk

SRC_CC += fenv-softfloat.cc

INC_DIR += $(REP_DIR)/src/lib/libc/spec/riscv
INC_DIR += $(LIBC_DIR)/include/spec/riscv

CC_CXX_WARN_STRICT =
