#
# \brief  Portions of base library that are exclusive to non-core processes
# \author Norman Feske
# \date   2013-02-14
#

LIBS += base-common startup

SRC_CC += console/log_console.cc
SRC_CC += cpu/cache.cc
SRC_CC += env/env.cc env/context_area.cc env/reinitialize.cc
SRC_CC += thread/thread.cc thread_support.cc

INC_DIR += $(BASE_DIR)/src/base/env

vpath %.cc  $(REP_DIR)/src/base
vpath %.cc $(BASE_DIR)/src/base
