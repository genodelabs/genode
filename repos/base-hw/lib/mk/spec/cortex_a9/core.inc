#
# \brief  Build config for Genodes core process
# \author Stefan Kalkowski
# \date   2014-09-02
#

# add include paths
INC_DIR += $(BASE_DIR)/../base-hw/src/core/include/spec/cortex_a9
INC_DIR += $(BASE_DIR)/../base-hw/src/core/include/spec/arm_gic

# add C++ sources
SRC_CC += spec/cortex_a9/kernel/cpu.cc
SRC_CC += spec/cortex_a9/cpu.cc
SRC_CC += spec/cortex_a9/fpu.cc
SRC_CC += spec/arm/smp/kernel/thread_update_pd.cc
SRC_CC += spec/arm/smp/kernel/cpu.cc
SRC_CC += spec/arm/kernel/cpu_context.cc
SRC_CC += spec/arm_gic/pic.cc
SRC_CC += kernel/vm_thread_off.cc

# add Assembler sources
SRC_S  += spec/arm/smp/kernel/crt0.s

# include less specific configuration
include $(BASE_DIR)/../base-hw/lib/mk/spec/smp/core.inc
include $(BASE_DIR)/../base-hw/lib/mk/spec/arm_v7/core.inc
