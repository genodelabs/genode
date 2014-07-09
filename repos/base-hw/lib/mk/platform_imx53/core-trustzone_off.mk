#
# \brief  Build config for parts of core that depend on Trustzone status
# \author Stefan Kalkowski
# \author Martin Stein
# \date   2012-10-24
#

# add include paths
INC_DIR += $(REP_DIR)/src/core/include/spec/imx53
INC_DIR += $(REP_DIR)/src/core/include/spec/cortex_a8

# add C++ sources
SRC_CC += spec/imx53/platform_support.cc
SRC_CC += spec/imx53/pic.cc
SRC_CC += platform_services.cc

# include less specific configuration
include $(REP_DIR)/lib/mk/core-trustzone.inc
