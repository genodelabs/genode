SRC_CC = context_area.cc thread_roottask.cc thread.cc
LIBS  += lock thread_context

vpath thread.cc          $(BASE_DIR)/src/base/thread
vpath thread_roottask.cc $(REP_DIR)/src/test
vpath context_area.cc    $(REP_DIR)/src/test
