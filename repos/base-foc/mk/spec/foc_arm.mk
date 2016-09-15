#
# Specifics for Fiasco.OC on ARM
#

SPECS += foc

#
# ARM-specific L4/sys headers
#
L4_INC_DIR  = $(BUILD_BASE_DIR)/include/arm
L4F_INC_DIR = $(BUILD_BASE_DIR)/include/arm/l4f

#
# Defines for L4/sys headers
#
CC_OPT += -DCONFIG_L4_CALL_SYSCALLS -DARCH_arm

#
# Architecture-specific L4sys header files
#
L4_INC_TARGETS = arm/l4/sys \
                 arm/l4f/l4/sys \
                 arm/l4/vcpu

include $(call select_from_repositories,mk/spec/foc.mk)

INC_DIR += $(L4F_INC_DIR) $(L4_INC_DIR)
