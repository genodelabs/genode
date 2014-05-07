SRC_CC = foc_arm_low.cc
SRC_C  = reg-arm.c \
         linux-arm-low.c

CC_OPT_linux-arm-low += -Wno-unused-function

include $(REP_DIR)/lib/mk/gdbserver_platform.inc
