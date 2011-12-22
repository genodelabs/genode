LIBS     = syscalls
SRC_CC  += ipc.cc pager.cc
INC_DIR += $(REP_DIR)/src/base/lock

vpath %.cc $(REP_DIR)/src/base/ipc
