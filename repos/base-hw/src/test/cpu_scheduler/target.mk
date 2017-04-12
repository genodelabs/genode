#
# \brief  Build config for a core that tests its CPU-scheduler implementation
# \author Martin Stein
# \date   2011-12-16
#

TARGET   = test-cpu_scheduler
SRC_CC   = test.cc cpu_scheduler.cc double_list.cc
INC_DIR  = $(REP_DIR)/src/core $(REP_DIR)/src/lib $(BASE_DIR)/src/include
LIBS     = base

vpath test.cc $(PRG_DIR)
vpath %.cc    $(REP_DIR)/src/core/kernel
