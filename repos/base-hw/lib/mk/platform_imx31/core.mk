#
# \brief  Build config for Genodes core process
# \author Martin Stein
# \date   2012-10-04
#

# add include paths
INC_DIR += $(REP_DIR)/src/core/include/spec/imx31
INC_DIR += $(REP_DIR)/src/core/include/spec/imx

# add C++ sources
SRC_CC += platform_services.cc
SRC_CC += spec/imx31/platform_support.cc

# include less specific configuration
include $(REP_DIR)/lib/mk/arm_v6/core.inc
