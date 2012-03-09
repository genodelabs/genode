REQUIRES = linux
SRC_CC   = ipc.cc
LIBS     = syscall rpath cap_copy

vpath ipc.cc $(REP_DIR)/src/base/ipc
