LIBC_GEN_ARM_DIR = $(LIBC_DIR)/lib/libc/arm/gen

SRC_S   = _setjmp.S setjmp.S

#
# Remove 'longjmperror' call
#
CC_OPT += -D_STANDALONE

#
# Needed to compile on hard-float Linux
# FIXME: Floating point registers don't get saved/restored
#        when using this definition!
#
CC_OPT += -D__SOFTFP__

include $(REP_DIR)/lib/mk/libc-common.inc

vpath %.S $(LIBC_GEN_ARM_DIR)

CC_CXX_WARN_STRICT =
