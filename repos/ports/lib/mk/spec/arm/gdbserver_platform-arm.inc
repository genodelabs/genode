INC_DIR += $(REP_DIR)/src/lib/gdbserver_platform/spec/arm

SRC_C  = reg-arm.c \
         linux-arm-low.c

CC_OPT_linux-arm-low += -Wno-unused-function

include $(REP_DIR)/lib/mk/gdbserver_platform.inc
