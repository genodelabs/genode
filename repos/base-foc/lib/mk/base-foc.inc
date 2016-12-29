include $(BASE_DIR)/lib/mk/base.inc

LIBS   += base-foc-common syscall-foc cxx

SRC_CC += cap_map_remove.cc cap_alloc.cc
SRC_CC += thread_start.cc
