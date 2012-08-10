REQUIRES = linux
SRC_CC   = ipc.cc
LIBS     = syscall cap_copy
INC_DIR += $(REP_DIR)/src/base/ipc

vpath ipc.cc $(REP_DIR)/src/base/ipc
