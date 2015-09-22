include $(REP_DIR)/lib/mk/libc-gen.inc

LIBC_GEN_ARM_DIR = $(LIBC_DIR)/lib/libc/arm/gen

#FILTER_OUT_S += rfork_thread.S sigsetjmp.S setjmp.S _setjmp.S divsi3.S
FILTER_OUT_S += rfork_thread.S divsi3.S setjmp.S _setjmp.S
FILTER_OUT_C += _set_tp.c fabs.c frexp.c modf.c

SRC_S  += $(filter-out $(FILTER_OUT_S),$(notdir $(wildcard $(LIBC_GEN_ARM_DIR)/*.S)))
SRC_C  += $(filter-out $(FILTER_OUT_C),$(notdir $(wildcard $(LIBC_GEN_ARM_DIR)/*.c)))

# fix missing include prefix for 'ucontext.h', should be 'sys/ucontext.h'
CC_OPT_makecontext = -I$(call select_from_ports,libc)/include/libc/sys

vpath % $(LIBC_GEN_ARM_DIR)
