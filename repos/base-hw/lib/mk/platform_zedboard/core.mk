#
# \brief  Build config for Genodes core process
# \author Mark Albers
# \author Johannes Schlatow
# \date   2015-03-12
#

# add include paths
INC_DIR += $(REP_DIR)/src/core/include/spec/zedboard
INC_DIR += $(REP_DIR)/src/core/include/spec/xilinx

# add C++ sources
SRC_CC += platform_services.cc
SRC_CC += spec/zynq/platform_support.cc

# include less specific configuration
include $(REP_DIR)/lib/mk/cortex_a9/core.inc
