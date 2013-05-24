include $(REP_DIR)/lib/mk/base.inc

LIBS += startup cxx

SRC_CC += thread.cc thread_linux.cc

vpath thread.cc      $(BASE_DIR)/src/base/thread
vpath thread_linux.cc $(REP_DIR)/src/base/thread
