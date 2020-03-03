#
# \brief  Build config for Genodes core process
# \author Stefan Kalkowski
# \date   2014-09-02
#

# add include paths
INC_DIR += $(BASE_DIR)/../base-hw/src/core/spec/cortex_a8

# add C++ sources
SRC_CC += kernel/cpu_up.cc
SRC_CC += kernel/lock.cc

NR_OF_CPUS = 1

# include less specific configuration
include $(BASE_DIR)/../base-hw/lib/mk/spec/arm_v7/core-hw.inc
