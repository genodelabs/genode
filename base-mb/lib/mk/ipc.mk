SRC_CC  = ipc.cc ipc_marshal_cap.cc
SRC_CC += pager.cc
LIBS   += thread_context cap_copy

vpath ipc.cc              $(REP_DIR)/src/base/ipc
vpath pager.cc            $(REP_DIR)/src/base/ipc
vpath ipc_marshal_cap.cc $(BASE_DIR)/src/base/ipc
