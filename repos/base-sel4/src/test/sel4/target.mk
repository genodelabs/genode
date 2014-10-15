TARGET = test-sel4
SRC_CC = main.cc context_area.cc mini_env.cc thread.cc

LIBS   = base-common

SRC_CC += base/console/core_printf.cc
INC_DIR += $(REP_DIR)/src/base/console

vpath %.cc $(BASE_DIR)/src

SRC_CC += thread_sel4.cc
vpath thread_sel4.cc $(REP_DIR)/src/base/thread

SRC_CC += thread_bootstrap.cc
vpath thread_bootstrap.cc $(REP_DIR)/src/base/thread
