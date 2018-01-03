include $(REP_DIR)/lib/mk/libm.mk

SRC_C += msun/i387/fenv.c

vpath msun/i387/fenv.c $(LIBC_DIR)/lib

CC_CXX_WARN_STRICT =
