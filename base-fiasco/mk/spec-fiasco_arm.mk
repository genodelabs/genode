#
# Specifics for Fiasco on ARM
#
# The following variables must be defined by a platform spec file:
#
#   L4SYS_ARM_CPU - Platform identifiert used for constructing l4sys path
#                   names corresponding to the ARM platform. For example,
#                   specify 'arm_int' for the ARM integrator board.
#   RAM_BASE      - Start address of physical memory. If not specified,
#                   the start adress 0x0 is used.
#

SPECS += arm fiasco 32bit

#
# ARM-specific L4/sys headers
#
L4_INC_DIR  += $(L4_BUILD_DIR)/include/arm/l4v2 \
               $(L4_BUILD_DIR)/include/arm

#
# Support for Fiasco's ARM-specific extensions of L4
# and ARM-specific utility functions.
#
REP_INC_DIR += include/arm

#
# Defines for L4/sys headers
#
CC_OPT += -DSYSTEM_$(L4SYS_ARM_CPU)_l4v2
CC_OPT += -DCONFIG_L4_CALL_SYSCALLS -DL4API_l4v2 -DARCH_arm
CC_OPT += -msoft-float -fomit-frame-pointer
AS_OPT += -mfpu=softfpa

#
# Linker options that are specific for L4 on ARM
#
RAM_BASE     ?= 0x0
LD_TEXT_ADDR ?= $(shell printf "0x%x" $$(($(RAM_BASE) + 0x00078000)))
CXX_LINK_OPT += -Wl,-Ttext=$(LINK_TEXT_ADDR)
CXX_LINK_OPT += -L$(L4_BUILD_DIR)/lib/$(L4SYS_ARM_CPU)/l4v2
EXT_OBJECTS  += -ll4sys

#
# Also include less-specific configuration last
#
include $(call select_from_repositories,mk/spec-32bit.mk)
include $(call select_from_repositories,mk/spec-fiasco.mk)

INC_DIR      += $(L4_INC_DIR)
