include $(BASE_DIR)/lib/mk/base.inc

LIBS   += base-okl4-common syscall-okl4
SRC_CC += thread_start.cc
SRC_CC += cache.cc
SRC_CC += capability_space.cc
