#
# \brief  Build config for Genodes core process
# \author Stefan Kalkowski
# \date   2013-11-25
#

# add include paths
INC_DIR += $(REP_DIR)/src/core/include/spec/odroid_xu

# add C++ sources
SRC_CC += spec/odroid_xu/platform_support.cc
SRC_CC += cpu.cc

# include less specific library parts
include $(REP_DIR)/lib/mk/exynos5/core.inc
