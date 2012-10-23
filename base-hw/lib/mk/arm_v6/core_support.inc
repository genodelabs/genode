#
# \brief  Parts of core that depend on ARMv6
# \author Martin Stein
# \date   2012-04-16
#

# add assembly sources
SRC_S += mode_transition.s

# declare source paths
vpath % $(REP_DIR)/src/core/arm_v6

# include less specific parts
include $(REP_DIR)/lib/mk/arm/core_support.inc

