LIBM_LD128 = yes

include $(REP_DIR)/lib/mk/libm.inc

SRC_C += msun/aarch64/fenv.c

vpath msun/aarch64/fenv.c $(LIBC_DIR)/lib
