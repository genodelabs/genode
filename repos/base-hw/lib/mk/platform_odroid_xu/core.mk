#
# \brief  Build config for Genodes core process
# \author Stefan Kalkowski
# \date   2013-11-25
#

# add include paths
INC_DIR += $(REP_DIR)/src/core/odroid_xu

# declare source paths
vpath platform_support.cc $(REP_DIR)/src/core/odroid_xu

# include less specific library parts
include $(REP_DIR)/lib/mk/exynos5/core.inc
