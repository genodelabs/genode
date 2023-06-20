include $(BASE_DIR)/lib/mk/base.inc

SRC_CC += capability_space.cc capability_slab.cc
SRC_CC += thread_start.cc thread_init.cc
SRC_CC += cache.cc
SRC_CC += signal_transmitter.cc signal.cc

LIBS += syscall-sel4 base-sel4-common cxx
