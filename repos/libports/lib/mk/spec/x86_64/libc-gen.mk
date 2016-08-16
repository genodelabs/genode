include $(REP_DIR)/lib/mk/libc-gen.inc

LIBC_GEN_AMD64_DIR = $(LIBC_DIR)/lib/libc/amd64/gen

FILTER_OUT_S += rfork_thread.S setjmp.S _setjmp.S
FILTER_OUT_C += _set_tp.c

#
# Filter functions conflicting with libm
#
FILTER_OUT_S += fabs.S modf.S
FILTER_OUT_C += frexp.c

SRC_S  = $(filter-out $(FILTER_OUT_S),$(notdir $(wildcard $(LIBC_GEN_AMD64_DIR)/*.S)))
SRC_C += $(filter-out $(FILTER_OUT_C),$(notdir $(wildcard $(LIBC_GEN_AMD64_DIR)/*.c)))

vpath %.c $(LIBC_GEN_AMD64_DIR)
vpath %.S $(LIBC_GEN_AMD64_DIR)
