#
# \brief  Build config for Genodes core process
# \author Stefan Kalkowski
# \date   2014-09-02
#

TMP         := $(call select_from_repositories,lib/mk/bootstrap-hw.inc)
BASE_HW_DIR := $(TMP:%lib/mk/bootstrap-hw.inc=%)

# add include paths
INC_DIR += $(BASE_HW_DIR)/src/core/spec/cortex_a8

# add C++ sources
SRC_CC += kernel/cpu_up.cc
SRC_CC += kernel/lock.cc

NR_OF_CPUS = 1

# include less specific configuration
include $(BASE_HW_DIR)/lib/mk/spec/arm_v7/core-hw.inc
