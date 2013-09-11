#
# \brief  Portions of base library that are exclusive to non-core processes
# \author Norman Feske
# \date   2013-02-14
#

LIBS += base-common

SRC_CC += console/log_console.cc
SRC_CC += env/env.cc env/main_thread.cc \
          env/context_area.cc env/reload_parent_cap
SRC_CC += thread/thread_nova.cc

INC_DIR += $(BASE_DIR)/src/base/env

vpath %.cc  $(REP_DIR)/src/base
vpath %.cc $(BASE_DIR)/src/base
