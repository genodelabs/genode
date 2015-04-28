#
# \brief  Portions of base library that are exclusive to non-core processes
# \author Norman Feske
# \date   2013-02-14
#

# add library dependencies
LIBS += base-common startup

# add C++ sources
SRC_CC += console/log_console.cc
SRC_CC += cpu/cache.cc
SRC_CC += env/env.cc
SRC_CC += env/context_area.cc
SRC_CC += env/reinitialize.cc
SRC_CC += thread/start.cc
SRC_CC += irq/platform.cc

# add include paths
INC_DIR += $(BASE_DIR)/src/base/env

# declare source locations
vpath %  $(REP_DIR)/src/base
vpath % $(BASE_DIR)/src/base
