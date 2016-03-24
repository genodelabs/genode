#
# \brief  Build config for Genodes core process
# \author Johannes Schlatow
# \date   2014-12-15
#

# add include paths
INC_DIR += $(REP_DIR)/src/core/include/spec/xilinx_uartps_1


# include less specific configuration
include $(REP_DIR)/lib/mk/spec/zynq/core.inc
