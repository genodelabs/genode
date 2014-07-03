#
# \brief  Build config for Genodes core process
# \author Martin Stein
# \date   2012-10-04
#

# add include paths
INC_DIR += $(REP_DIR)/src/core/arm_v6

# declare source paths
vpath mode_transition.s $(REP_DIR)/src/core/arm_v6

# include less specific library parts
include $(REP_DIR)/lib/mk/arm/core.inc
