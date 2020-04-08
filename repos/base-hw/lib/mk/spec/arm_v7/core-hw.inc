#
# \brief  Build config for Genodes core process
# \author Martin Stein
# \date   2011-12-16
#

TMP         := $(call select_from_repositories,lib/mk/bootstrap-hw.inc)
BASE_HW_DIR := $(TMP:%lib/mk/bootstrap-hw.inc=%)

# add include paths
INC_DIR += $(BASE_HW_DIR)/src/core/spec/arm_v7

# add C++ sources
SRC_CC += spec/arm_v7/perf_counter.cc

SRC_S  += spec/arm/vfpv3-d32.cc

# include less specific configuration
include $(BASE_HW_DIR)/lib/mk/spec/arm/core-hw.inc
