LIBM_LD128 = yes

include $(REP_DIR)/lib/mk/libm.inc

#
# RISC-V specific
#
SRC_C += msun/riscv/fenv.c

vpath msun/riscv/fenv.c $(LIBC_DIR)/lib
