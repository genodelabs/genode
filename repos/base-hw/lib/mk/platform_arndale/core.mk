#
# \brief  Build config for Genodes core process
# \author Stefan Kalkowski
# \author Martin Stein
# \date   2012-10-04
#

# add include paths
INC_DIR += $(REP_DIR)/src/core/include/spec/arndale

# add C++ sources
SRC_CC += spec/arndale/platform_support.cc
SRC_CC += spec/arndale/cpu.cc

# include less specific configuration
include $(REP_DIR)/lib/mk/exynos5/core.inc
