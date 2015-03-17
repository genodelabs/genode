SRC_CC += console/log_console.cc
SRC_CC += cpu/cache.cc
SRC_CC += env/env.cc env/context_area.cc env/reinitialize.cc
SRC_CC += thread/thread_start.cc
SRC_CC += irq/platform.cc

INC_DIR += $(BASE_DIR)/src/base/env
INC_DIR += $(REP_DIR)/include/codezero/dummies

LIBS += base-common

vpath %.cc  $(REP_DIR)/src/base
vpath %.cc $(BASE_DIR)/src/base

