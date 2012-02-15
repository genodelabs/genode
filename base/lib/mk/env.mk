SRC_CC  = env.cc context_area.cc reload_parent_cap.cc
LIBS    = ipc heap log_console lock

vpath env.cc                $(REP_DIR)/src/base/env
vpath context_area.cc      $(BASE_DIR)/src/base/env
vpath reload_parent_cap.cc $(BASE_DIR)/src/base/env
