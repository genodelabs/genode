#
# \brief  Build config for Genodes core process
# \author Stefan Kalkowski
# \author Martin Stein
# \date   2012-10-04
#

# add include paths
INC_DIR += $(REP_DIR)/src/core/arndale

# declare source paths
vpath platform_support.cc $(REP_DIR)/src/core/arndale

# include less specific library parts
include $(REP_DIR)/lib/mk/exynos5/core.inc
