#
# \brief  Build config for Genodes core process
# \author Stefan Kalkowski
# \author Martin Stein
# \date   2012-10-24
#

# add include paths
INC_DIR += $(REP_DIR)/src/core/include/spec/imx53
INC_DIR += $(REP_DIR)/src/core/include/spec/imx

# include less specific configuration
include $(REP_DIR)/lib/mk/spec/cortex_a8/core-hw.inc
