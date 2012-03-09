SRC_CC   = ipc.cc pager.cc
INC_DIR += $(REP_DIR)/include/codezero/dummies
LIBS     = cap_copy

vpath %.cc $(REP_DIR)/src/base/ipc
