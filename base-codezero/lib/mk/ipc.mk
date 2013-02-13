SRC_CC   = ipc.cc pager.cc ipc_marshal_cap.cc
INC_DIR += $(REP_DIR)/include/codezero/dummies
LIBS     = cap_copy

vpath %.cc                $(REP_DIR)/src/base/ipc
vpath ipc_marshal_cap.cc $(BASE_DIR)/src/base/ipc
