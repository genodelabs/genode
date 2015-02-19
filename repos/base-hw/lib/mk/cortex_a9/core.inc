#
# \brief  Build config for Genodes core process
# \author Stefan Kalkowski
# \date   2014-09-02
#

# add include paths
INC_DIR += $(REP_DIR)/src/core/include/spec/cortex_a9
INC_DIR += $(REP_DIR)/src/core/include/spec/arm_gic

# add C++ sources
SRC_CC += spec/arm/cpu.cc
SRC_CC += spec/arm_gic/pic.cc
SRC_CC += spec/arm/kernel/cpu_context.cc
SRC_CC += kernel/vm_thread.cc

# include less specific configuration
include $(REP_DIR)/lib/mk/arm_v7/core.inc
