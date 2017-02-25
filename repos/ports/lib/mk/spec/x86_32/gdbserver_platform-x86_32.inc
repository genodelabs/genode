INC_DIR += $(REP_DIR)/src/lib/gdbserver_platform/spec/x86_32

SRC_C   = i386.c \
          i386-avx.c \
          linux-x86-low.c

CC_OPT_linux-x86-low += -Wno-unused-function

include $(REP_DIR)/lib/mk/gdbserver_platform.inc
