#
# \brief  Portions of base library that are exclusive to non-core components
# \author Norman Feske
# \date   2013-02-14
#

LIBS += base-common startup

SRC_CC += console/log_console.cc
SRC_CC += cpu/cache.cc
SRC_CC += env/env.cc
SRC_CC += env/stack_area.cc
SRC_CC += env/reinitialize.cc
SRC_CC += thread/start.cc
SRC_CC += irq/platform.cc
SRC_CC += env.cc
SRC_CC += capability.cc
SRC_CC += server/rpc_cap_alloc.cc

INC_DIR += $(REP_DIR)/src/include $(BASE_DIR)/src/include

vpath %  $(REP_DIR)/src/base
vpath % $(BASE_DIR)/src/base
