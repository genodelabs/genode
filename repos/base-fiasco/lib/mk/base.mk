#
# \brief  Portions of base library that are exclusive to non-core processes
# \author Norman Feske
# \date   2013-02-14
#

LIBS += base-common

SRC_CC += console/log_console.cc
SRC_CC += cpu/cache.cc
SRC_CC += env/env.cc env/context_area.cc env/reinitialize.cc
SRC_CC += thread/thread_start.cc
SRC_CC += irq/platform.cc
SRC_CC += server/rpc_cap_alloc.cc

INC_DIR += $(REP_DIR)/src/include $(BASE_DIR)/src/include

vpath %.cc  $(REP_DIR)/src/base
vpath %.cc $(BASE_DIR)/src/base
