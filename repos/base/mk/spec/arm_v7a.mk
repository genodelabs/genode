#
# \brief  Build-system configurations for ARMv7a
# \author Martin Stein
# \date   2012-09-26
#

# denote wich specs are also fullfilled by this spec
SPECS += arm_v7

# add repository relative include paths
REP_INC_DIR += include/spec/arm_v7a

# include implied specs
include $(BASE_DIR)/mk/spec/arm_v7.mk

