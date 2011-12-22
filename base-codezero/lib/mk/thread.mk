SRC_CC   = thread.cc thread_start.cc thread_bootstrap.cc
INC_DIR += $(REP_DIR)/include/codezero/dummies

vpath thread_start.cc $(REP_DIR)/src/base/thread
vpath %.cc           $(BASE_DIR)/src/base/thread
