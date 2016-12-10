#
# \brief  Core build-config that depends on performance-counter status
# \author Josef Soentgen
# \date   2013-09-26
#

# add include paths
INC_DIR += $(REP_DIR)/src/core/include

# add C++ sources
SRC_CC += perf_counter.cc

# declare source locations
vpath % $(REP_DIR)/src/core
