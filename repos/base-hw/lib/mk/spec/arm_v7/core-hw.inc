#
# \brief  Build config for Genodes core process
# \author Martin Stein
# \date   2011-12-16
#

# add include paths
INC_DIR += $(BASE_DIR)/../base-hw/src/core/spec/arm_v7

# add C++ sources
SRC_CC += spec/arm_v7/cpu.cc
SRC_CC += spec/arm_v7/perf_counter.cc

# include less specific configuration
include $(BASE_DIR)/../base-hw/lib/mk/spec/arm/core-hw.inc
