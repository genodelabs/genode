include $(REP_DIR)/lib/mk/libc-gen.inc

LIBC_GEN_I386_DIR = $(LIBC_DIR)/libc/i386/gen

FILTER_OUT_S += rfork_thread.S sigsetjmp.S _setjmp.S setjmp.S
FILTER_OUT_C += _set_tp.c

#
# Filter functions conflicting with libm
#
FILTER_OUT_S += fabs.S modf.S
FILTER_OUT_C += frexp.c

SRC_S  = $(filter-out $(FILTER_OUT_S),$(notdir $(wildcard $(LIBC_GEN_I386_DIR)/*.S)))
SRC_C += flt_rounds.c

#
# Additional functions for shared libraries
#
SRC_C += makecontext.c

vpath %.c $(LIBC_GEN_I386_DIR)
vpath %.S $(LIBC_GEN_I386_DIR)
