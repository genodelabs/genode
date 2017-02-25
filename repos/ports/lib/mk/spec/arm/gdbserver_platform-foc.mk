REQUIRES += foc

SRC_CC = spec/arm/low.cc \
         native_cpu.cc

LIBS += syscall-foc

SHARED_LIB = yes

vpath %.cc $(REP_DIR)/src/lib/gdbserver_platform-foc

include $(REP_DIR)/lib/mk/spec/arm/gdbserver_platform-arm.inc
