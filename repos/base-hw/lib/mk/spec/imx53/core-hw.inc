#
# \brief  Build config for Genodes core process
# \author Stefan Kalkowski
# \author Martin Stein
# \date   2012-10-24
#

# add include paths
INC_DIR += $(REP_DIR)/src/core/spec/imx53
INC_DIR += $(REP_DIR)/src/core/spec/imx

SRC_CC += spec/imx53/pic.cc
SRC_CC += spec/arm/cpu_context_trustzone.cc

# include less specific configuration
include $(REP_DIR)/lib/mk/spec/cortex_a8/core-hw.inc
