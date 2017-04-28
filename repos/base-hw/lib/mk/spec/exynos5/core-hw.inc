#
# \brief  Build config for Genodes core process
# \author Martin Stein
# \date   2011-12-16
#

# add include paths
INC_DIR += $(BASE_DIR)/../base-hw/src/core/spec/exynos5

# add C++ sources
SRC_CC += spec/exynos5/cpu.cc
SRC_CC += spec/exynos5/timer.cc

# include less specific configuration
include $(BASE_DIR)/../base-hw/lib/mk/spec/cortex_a15/core-hw.inc
