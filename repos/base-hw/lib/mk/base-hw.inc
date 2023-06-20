include $(BASE_DIR)/lib/mk/base.inc

SRC_CC += log.cc
SRC_CC += thread_start.cc
SRC_CC += capability.cc
SRC_CC += capability_slab.cc
SRC_CC += cache.cc
SRC_CC += raw_write_string.cc
SRC_CC += signal_receiver.cc
SRC_CC += stack_area_addr.cc
SRC_CC += native_utcb.cc

LIBS += startup-hw base-hw-common syscall-hw cxx timeout-hw
