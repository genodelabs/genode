LIBM_LD80 = yes

include $(REP_DIR)/lib/mk/libm.inc

SRC_C += msun/amd64/fenv.c

vpath msun/amd64/fenv.c $(LIBC_DIR)/lib
