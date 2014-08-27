#
# \brief  Build config for Genodes core process
# \author Martin Stein
# \date   2011-12-16
#

# add include paths
INC_DIR += $(REP_DIR)/src/core/include/spec/arm_v7

# add assembly sources
SRC_S += spec/arm_v7/mode_transition.s

# include less specific configuration
include $(REP_DIR)/lib/mk/arm/core.inc
