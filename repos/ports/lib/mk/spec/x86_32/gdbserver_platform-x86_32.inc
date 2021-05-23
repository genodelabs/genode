INC_DIR += $(REP_DIR)/src/lib/gdbserver_platform/spec/x86_32

SRC_CC += arch/i386.cc \
          linux-x86-low.cc \
          linux-x86-tdesc.cc \
          nat/x86-linux.cc \
          nat/x86-linux-dregs.cc

CC_OPT += -fpermissive -Wno-unused-function

include $(REP_DIR)/lib/mk/gdbserver_platform.inc
