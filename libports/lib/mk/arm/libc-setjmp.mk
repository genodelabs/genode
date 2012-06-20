LIBC_GEN_ARM_DIR = $(LIBC_DIR)/libc/arm/gen

SRC_S   = _setjmp.S setjmp.S

#
# Remove 'longjmperror' call
#
CC_OPT += -D_STANDALONE

include $(REP_DIR)/lib/mk/libc-common.inc

vpath %.S $(LIBC_GEN_ARM_DIR)
