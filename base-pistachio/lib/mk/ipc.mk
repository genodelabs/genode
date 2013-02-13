SRC_CC  = ipc.cc pager.cc ipc_marshal_cap.cc
LIBS    = cap_copy

# disable warning about array boundaries, caused by L4 headers
CC_WARN = -Wall -Wno-array-bounds

vpath %.cc                $(REP_DIR)/src/base/ipc
vpath ipc_marshal_cap.cc $(BASE_DIR)/src/base/ipc
