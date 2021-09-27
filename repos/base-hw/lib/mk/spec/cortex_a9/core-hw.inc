#
# \brief  Build config for Genodes core process
# \author Stefan Kalkowski
# \date   2014-09-02
#

# add include paths
REP_INC_DIR += src/core/spec/cortex_a9

# add C++ sources
SRC_CC += spec/cortex_a9/board.cc
SRC_CC += spec/cortex_a9/cpu.cc
SRC_CC += spec/arm/cortex_a9_private_timer.cc
SRC_CC += spec/arm/gicv2.cc
SRC_CC += spec/arm/kernel/lock.cc
SRC_CC += kernel/vm_thread_off.cc
SRC_CC += kernel/cpu_mp.cc

# include less specific configuration
include $(call select_from_repositories,lib/mk/spec/arm_v7/core-hw.inc)
