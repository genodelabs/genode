REQUIRES += nova

SRC_CC = spec/x86_64/low.cc \
         native_cpu.cc

SHARED_LIB = yes

vpath %.cc $(REP_DIR)/src/lib/gdbserver_platform-nova

include $(REP_DIR)/lib/mk/spec/x86_64/gdbserver_platform-x86_64.inc
