include $(REP_DIR)/lib/mk/libc-gen.inc

LIBC_GEN_RISCV_DIR = $(LIBC_DIR)/lib/libc/riscv/gen

SRC_S += _ctx_start.S sigsetjmp.S
SRC_C += flt_rounds.c fpgetmask.c fpsetmask.c infinity.c makecontext.c

#
# Fix missing include prefix for 'ucontext.h', should be 'sys/ucontext.h'
#
# The first path is in effect when using the regular build system. The second
# path is in effect when building the libc from a source archive (where the
# ucontext.h header is taken from the libc API archive).
#
CC_OPT_makecontext = -I$(LIBC_PORT_DIR)/include/libc/sys \
                     $(addprefix -I,$(call select_from_repositories,/include/libc/sys))

CC_OPT += -DSOFTFLOAT_FOR_GCC

vpath fpgetmask.c $(LIBC_DIR)/lib/libc/softfloat
vpath fpsetmask.c $(LIBC_DIR)/lib/libc/softfloat
vpath %.c $(LIBC_GEN_RISCV_DIR)
vpath %.S $(LIBC_GEN_RISCV_DIR)
