#
# \brief  Build config for Genodes core process
# \author Stefan Kalkowski
# \author Martin Stein
# \date   2012-10-24
#

# add library dependencies
LIBS += core-trustzone

# add include paths
INC_DIR += $(REP_DIR)/src/core/include/spec/imx53
INC_DIR += $(REP_DIR)/src/core/include/spec/imx
INC_DIR += $(REP_DIR)/src/core/include/spec/cortex_a8

# add C++ sources
SRC_CC += cpu.cc

# include less specific configuration
include $(REP_DIR)/lib/mk/arm_v7/core.inc
