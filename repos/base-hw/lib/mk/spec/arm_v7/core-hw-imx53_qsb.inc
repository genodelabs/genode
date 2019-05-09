#
# \brief  Build config for Genodes core process
# \author Stefan Kalkowski
# \author Martin Stein
# \date   2012-10-24
#

# add include paths
INC_DIR += $(REP_DIR)/src/core/spec/imx53_qsb
INC_DIR += $(REP_DIR)/src/core/spec/imx53

SRC_CC += spec/imx53/pic.cc
SRC_CC += spec/imx53/timer.cc

# include less specific configuration
include $(REP_DIR)/lib/mk/spec/cortex_a8/core-hw.inc

