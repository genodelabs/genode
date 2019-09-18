include $(REP_DIR)/lib/mk/libc.mk

INC_DIR += $(REP_DIR)/src/lib/libc/spec/x86_32
INC_DIR += $(LIBC_DIR)/include/spec/x86_32

SRC_C += msun/i387/fenv.c
vpath msun/i387/fenv.c $(LIBC_DIR)/lib

CC_CXX_WARN_STRICT =
