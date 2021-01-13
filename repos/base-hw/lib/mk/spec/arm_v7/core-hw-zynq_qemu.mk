#
# \brief  Build config for Genodes core process
# \author Johannes Schlatow
# \date   2014-12-15
#

# add include paths
INC_DIR += $(REP_DIR)/src/core/board/zynq_qemu

NR_OF_CPUS = 1

# include less specific configuration
include $(REP_DIR)/lib/mk/spec/arm_v7/core-hw-zynq.inc
