REQUIRES += nova

SRC_CC = spec/x86_32/low.cc \
         native_cpu.cc

SHARED_LIB = yes

vpath %.cc $(REP_DIR)/src/lib/gdbserver_platform-nova

include $(REP_DIR)/lib/mk/spec/x86_32/gdbserver_platform-x86_32.inc

CC_CXX_WARN_STRICT =
