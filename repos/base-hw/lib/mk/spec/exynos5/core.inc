#
# \brief  Build config for Genodes core process
# \author Martin Stein
# \date   2011-12-16
#

# add include paths
INC_DIR += $(BASE_HW_DIR)/src/core/include/spec/exynos5

# add C++ sources
SRC_CC += spec/exynos5/platform_support.cc
SRC_CC += spec/exynos5/cpu.cc

# include less specific configuration
include $(BASE_HW_DIR)/lib/mk/spec/cortex_a15/core.inc
