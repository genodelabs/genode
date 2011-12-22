REQUIRES = linux
SRC_CC   = thread.cc thread_linux.cc
LIBS     = syscall

vpath thread.cc       $(BASE_DIR)/src/base/thread
vpath thread_linux.cc $(REP_DIR)/src/base/thread
