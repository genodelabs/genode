#
# \brief  Build-system configurations for ARM Cortex A15
# \author Martin Stein
# \date   2013-01-09
#

# denote wich specs are also fullfilled by this spec
SPECS += arm_v7a

# add repository relative include paths
REP_INC_DIR += include/spec/cortex_a15

# configure compiler
#
# GCC versions up to 4.7.3 complain about conflicting command-line switches:
#
#   warning: switch -mcpu=cortex-a15 conflicts with -march=armv7-a switch [enabled by default]
#
# Therefore, do not specify the actual CPU and the architecture together.
#
# Reference: https://github.com/genodelabs/genode/issues/810
#
CC_MARCH += -mcpu=cortex-a15

# include implied specs
include $(BASE_DIR)/mk/spec/arm_v7a.mk
