#
# Specifics for Fiasco.OC on ARM
#

SPECS += foc

#
# Linker options that are specific for arm
#
LD_TEXT_ADDR ?= 0x01000000

#
# ARM-specific L4/sys headers
#
L4_INC_DIR  = $(BUILD_BASE_DIR)/include/arm
L4F_INC_DIR = $(BUILD_BASE_DIR)/include/arm/l4f

#
# Support for Fiasco.OC's ARM-specific atomic functions
#
REP_INC_DIR += include/arm

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

include $(call select_from_repositories,mk/spec-foc.mk)

INC_DIR += $(L4F_INC_DIR) $(L4_INC_DIR)
