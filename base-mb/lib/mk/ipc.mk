SRC_CC  = ipc.cc
SRC_CC += pager.cc
LIBS   += thread_context

vpath ipc.cc   $(REP_DIR)/src/base/ipc
vpath pager.cc $(REP_DIR)/src/base/ipc
