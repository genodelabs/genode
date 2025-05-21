include $(REP_DIR)/lib/mk/libc.mk

SRC_CC += fenv-softfloat.cc

INC_DIR += $(LIBC_DIR)/include/spec/riscv

vpath % $(REP_DIR)/src/lib/libc/spec/riscv

CC_CXX_WARN_STRICT =
