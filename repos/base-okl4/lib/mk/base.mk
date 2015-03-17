SRC_CC += console/log_console.cc
SRC_CC += cpu/cache.cc
SRC_CC += env/env.cc env/context_area.cc env/reinitialize.cc
SRC_CC += thread/thread_start.cc
SRC_CC += irq/platform.cc

vpath %.cc  $(REP_DIR)/src/base
vpath %.cc $(BASE_DIR)/src/base

INC_DIR += $(BASE_DIR)/src/base/env

LIBS += base-common
