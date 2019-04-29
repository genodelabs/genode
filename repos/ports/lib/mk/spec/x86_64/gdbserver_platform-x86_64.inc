INC_DIR += $(REP_DIR)/src/lib/gdbserver_platform/spec/x86_64

SRC_CC += arch/i386.cc \
          arch/amd64.cc \
          gdbserver/linux-x86-low.cc \
          gdbserver/linux-x86-tdesc.cc \
          nat/x86-linux.cc \
          nat/x86-linux-dregs.cc

CC_OPT += -fpermissive -Wno-unused-function

include $(REP_DIR)/lib/mk/gdbserver_platform.inc
