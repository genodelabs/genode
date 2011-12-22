SRC_CC   = env.cc rm_session_mmap.cc platform_env.cc debug.cc context_area.cc
LIBS     = ipc heap log_console lock syscall

vpath env.cc          $(BASE_DIR)/src/base/env
vpath context_area.cc $(BASE_DIR)/src/base/env
vpath %.cc             $(REP_DIR)/src/base/env
