#
# \brief  Build config for Genodes core process
# \author Stefan Kalkowski
# \date   2014-09-02
#

# add include paths
INC_DIR += $(BASE_DIR)/../base-hw/src/core/spec/cortex_a15
INC_DIR += $(BASE_DIR)/../base-hw/src/core/spec/arm_gic

# add C++ sources
SRC_CC += spec/cortex_a15/cpu.cc
SRC_CC += spec/cortex_a15/kernel/cpu.cc
SRC_CC += spec/arm/smp/kernel/thread_update_pd.cc
SRC_CC += spec/arm/smp/kernel/cpu.cc

# include less specific configuration
include $(BASE_DIR)/../base-hw/lib/mk/spec/smp/core-hw.inc
include $(BASE_DIR)/../base-hw/lib/mk/spec/arm_v7/core-hw.inc
