#
# \brief  Build config for Genodes core process
# \author Martin Stein
# \date   2011-12-16
#

# add include paths
INC_DIR += $(REP_DIR)/src/core/include/spec/exynos5
INC_DIR += $(REP_DIR)/src/core/include/spec/cortex_a15
INC_DIR += $(REP_DIR)/src/core/include/spec/corelink_gic400

# add C++ sources
SRC_CC += platform_services.cc
SRC_CC += spec/arm_gic/pic.cc

# include less specific configuration
include $(REP_DIR)/lib/mk/arm_v7/core.inc
