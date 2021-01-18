#
# \brief  Build config for Genodes core process
# \author Johannes Schlatow
# \date   2014-12-15
#

# add include paths
REP_INC_DIR += src/core/board/zynq_qemu

NR_OF_CPUS = 1

# include less specific configuration
include $(call select_from_repositories,lib/mk/spec/arm_v7/core-hw-zynq.inc)
