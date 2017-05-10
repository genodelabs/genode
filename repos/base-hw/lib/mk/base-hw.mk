include $(BASE_DIR)/lib/mk/base.inc

SRC_CC += thread_start.cc
SRC_CC += env.cc
SRC_CC += capability.cc
SRC_CC += cache.cc
SRC_CC += raw_write_string.cc
SRC_CC += signal_receiver.cc

LIBS += startup-hw base-hw-common cxx timeout-hw
