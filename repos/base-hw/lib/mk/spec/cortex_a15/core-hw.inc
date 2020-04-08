#
# \brief  Build config for Genodes core process
# \author Stefan Kalkowski
# \date   2014-09-02
#

TMP         := $(call select_from_repositories,lib/mk/bootstrap-hw.inc)
BASE_HW_DIR := $(TMP:%lib/mk/bootstrap-hw.inc=%)

# add include paths
INC_DIR += $(BASE_HW_DIR)/src/core/spec/cortex_a15

# add C++ sources
SRC_CC += spec/cortex_a15/cpu.cc
SRC_CC += kernel/cpu_mp.cc
SRC_CC += spec/arm/kernel/lock.cc

# include less specific configuration
include $(BASE_HW_DIR)/lib/mk/spec/arm_v7/core-hw.inc
