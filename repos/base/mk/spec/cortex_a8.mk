#
# \brief  Build-system configurations specifically for the ARM Cortex A8
# \author Stefan Kalkowski
# \date   2012-10-15
#

# denote wich specs are also fullfilled by this spec
SPECS += arm_v7a

# add repository relative include paths
REP_INC_DIR += include/spec/cortex_a8

# configure compiler
CC_MARCH += -march=armv7-a -mcpu=cortex-a8

# include implied specs
include $(BASE_DIR)/mk/spec/arm_v7a.mk
