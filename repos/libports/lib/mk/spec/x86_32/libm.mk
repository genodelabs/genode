LIBM_LD80 = yes

include $(REP_DIR)/lib/mk/libm.inc

SRC_C += msun/i387/fenv.c

vpath msun/i387/fenv.c $(LIBC_DIR)/lib
