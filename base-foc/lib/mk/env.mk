SRC_CC = env.cc context_area.cc cap_sel_alloc.cc
LIBS   = ipc heap log_console lock

vpath env.cc           $(BASE_DIR)/src/base/env
vpath context_area.cc  $(BASE_DIR)/src/base/env
vpath cap_sel_alloc.cc  $(REP_DIR)/src/base/env
