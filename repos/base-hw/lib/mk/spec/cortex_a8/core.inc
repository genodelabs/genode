#
# \brief  Build config for Genodes core process
# \author Stefan Kalkowski
# \date   2014-09-02
#

# add include paths
INC_DIR += $(REP_DIR)/src/core/include/spec/cortex_a8

# add C++ sources
SRC_CC += spec/arm/cpu.cc
SRC_CC += spec/arm/kernel/cpu_context.cc

# include less specific configuration
include $(REP_DIR)/lib/mk/spec/arm_v7/core.inc
