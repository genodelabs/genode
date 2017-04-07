#
# \brief  Build-system configurations for ARMv6
# \author Martin Stein
# \date   2012-09-26
#

# denote wich specs are also fullfilled by this spec
SPECS += arm

# configure compiler
CC_MARCH += -march=armv6

# add repository relative include paths
REP_INC_DIR += include/spec/arm_v6

# include implied specs
include $(BASE_DIR)/mk/spec/arm.mk

