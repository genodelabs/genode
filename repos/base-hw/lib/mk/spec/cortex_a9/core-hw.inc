#
# \brief  Build config for Genodes core process
# \author Stefan Kalkowski
# \date   2014-09-02
#

# add include paths
INC_DIR += $(BASE_DIR)/../base-hw/src/core/spec/cortex_a9

# add C++ sources
SRC_CC += spec/cortex_a9/board.cc
SRC_CC += spec/cortex_a9/timer.cc
SRC_CC += spec/arm/gicv2.cc
SRC_CC += spec/arm/kernel/lock.cc
SRC_CC += kernel/vm_thread_off.cc
SRC_CC += kernel/cpu_mp.cc
SRC_CC += kernel/kernel.cc

# include less specific configuration
include $(BASE_DIR)/../base-hw/lib/mk/spec/arm_v7/core-hw.inc
