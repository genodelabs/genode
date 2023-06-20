include $(BASE_DIR)/lib/mk/base.inc

LIBS += syscall-fiasco base-fiasco-common cxx timeout

SRC_CC += thread_start.cc
SRC_CC += cache.cc
SRC_CC += capability_slab.cc
SRC_CC += capability_space.cc
SRC_CC += signal_transmitter.cc signal.cc
