SRC_CC = env.cc parent.cc context_area.cc
LIBS   = ipc heap lock log_console

vpath env.cc          $(BASE_DIR)/src/base/env
vpath context_area.cc $(BASE_DIR)/src/base/env
vpath parent.cc        $(REP_DIR)/src/base/env
