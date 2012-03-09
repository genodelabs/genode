SRC_CC   = ipc.cc pager.cc
LIBS     = thread_context cap_copy
INC_DIR += $(REP_DIR)/src/platform

vpath ipc.cc   $(REP_DIR)/src/base/ipc
vpath pager.cc $(REP_DIR)/src/base/ipc
