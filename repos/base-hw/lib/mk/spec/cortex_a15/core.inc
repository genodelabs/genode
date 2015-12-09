#
# \brief  Build config for Genodes core process
# \author Stefan Kalkowski
# \date   2014-09-02
#

# add include paths
INC_DIR += $(REP_DIR)/src/core/include/spec/cortex_a15
INC_DIR += $(REP_DIR)/src/core/include/spec/arm_gic
INC_DIR += $(REP_DIR)/src/core/include/spec/smp

# add C++ sources
SRC_CC += spec/cortex_a15/cpu.cc
SRC_CC += spec/arm_v7/smp/kernel/cpu.cc
SRC_CC += spec/smp/kernel/kernel.cc

# add assembler sources
SRC_S  += spec/arm/smp/kernel/crt0.s

# include less specific configuration
include $(REP_DIR)/lib/mk/spec/arm_v7/core.inc
