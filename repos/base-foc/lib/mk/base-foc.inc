include $(BASE_DIR)/lib/mk/base.inc

LIBS   += base-foc-common syscall-foc cxx timeout

SRC_CC += cap_map_remove.cc cap_alloc.cc
SRC_CC += thread_start.cc
SRC_CC += signal_transmitter.cc signal.cc
