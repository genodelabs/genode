REQUIRES += foc

SRC_CC = spec/x86_32/low.cc \
         native_cpu.cc

LIBS += syscall-foc

SHARED_LIB = yes

vpath %.cc $(REP_DIR)/src/lib/gdbserver_platform-foc

include $(REP_DIR)/lib/mk/spec/x86_32/gdbserver_platform-x86_32.inc
