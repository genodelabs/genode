#
# \brief  Build-system configurations specifically for the ARM Cortex A9
# \author Martin Stein
# \date   2011-12-20
#

# denote wich specs are also fullfilled by this spec
SPECS += arm_v7a

# add repository relative include paths
REP_INC_DIR += include/spec/cortex_a9

# configure compiler
CC_MARCH += -march=armv7-a -mcpu=cortex-a9

# include implied specs
include $(BASE_DIR)/mk/spec/arm_v7a.mk

