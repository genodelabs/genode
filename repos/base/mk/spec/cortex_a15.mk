SPECS += arm_v7a

REP_INC_DIR += include/spec/cortex_a15

include $(BASE_DIR)/mk/spec/arm_v7a.mk

# configure compiler
#
# GCC versions up to 4.7.3 complain about conflicting command-line switches:
#
#   warning: switch -mcpu=cortex-a15 conflicts with -march=armv7-a switch [enabled by default]
#
# Therefore, we override the 'CC_MARCH' of the included 'arm_v7a.mk'.
#
# Reference: https://github.com/genodelabs/genode/issues/810
#
CC_MARCH := -march=armv7ve -mcpu=cortex-a15
