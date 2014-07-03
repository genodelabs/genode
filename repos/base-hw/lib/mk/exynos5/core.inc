#
# \brief  Build config for Genodes core process
# \author Martin Stein
# \date   2011-12-16
#

# add include paths
INC_DIR += $(REP_DIR)/src/core/exynos5

# add C++ sources
SRC_CC += platform_services.cc
SRC_CC += platform_support.cc

# declare source paths
vpath platform_services.cc $(BASE_DIR)/src/core

# include less specific library parts
include $(REP_DIR)/lib/mk/arm_v7/core.inc
