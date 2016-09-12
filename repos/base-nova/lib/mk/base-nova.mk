include $(BASE_DIR)/lib/mk/base.inc

LIBS   += base-nova-common cxx
SRC_CC += thread_start.cc
SRC_CC += thread_no_execution_time.cc
SRC_CC += cache.cc
