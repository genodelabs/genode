include $(REP_DIR)/lib/mk/libm.mk

SRC_C += msun/amd64/fenv.c

vpath msun/amd64/fenv.c $(LIBC_DIR)/lib

CC_CXX_WARN_STRICT =
