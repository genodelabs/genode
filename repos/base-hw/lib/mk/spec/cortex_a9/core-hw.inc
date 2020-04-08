#
# \brief  Build config for Genodes core process
# \author Stefan Kalkowski
# \date   2014-09-02
#

TMP         := $(call select_from_repositories,lib/mk/bootstrap-hw.inc)
BASE_HW_DIR := $(TMP:%lib/mk/bootstrap-hw.inc=%)

# add include paths
INC_DIR += $(BASE_HW_DIR)/src/core/spec/cortex_a9

# add C++ sources
SRC_CC += spec/cortex_a9/board.cc
SRC_CC += spec/arm/cortex_a9_private_timer.cc
SRC_CC += spec/arm/gicv2.cc
SRC_CC += spec/arm/kernel/lock.cc
SRC_CC += kernel/vm_thread_off.cc
SRC_CC += kernel/cpu_mp.cc
SRC_CC += kernel/kernel.cc

# include less specific configuration
include $(BASE_HW_DIR)/lib/mk/spec/arm_v7/core-hw.inc
