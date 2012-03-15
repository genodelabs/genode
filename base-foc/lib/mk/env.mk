SRC_CC   = env.cc context_area.cc cap_map.cc cap_alloc.cc \
           reload_parent_cap.cc spin_lock.cc
LIBS     = ipc heap log_console lock
INC_DIR += $(REP_DIR)/src/base/lock $(BASE_DIR)/src/base/lock

vpath env.cc               $(BASE_DIR)/src/base/env
vpath context_area.cc      $(BASE_DIR)/src/base/env
vpath cap_map.cc           $(REP_DIR)/src/base/env
vpath cap_alloc.cc         $(REP_DIR)/src/base/env
vpath spin_lock.cc         $(REP_DIR)/src/base/env
vpath reload_parent_cap.cc $(BASE_DIR)/src/base/env
