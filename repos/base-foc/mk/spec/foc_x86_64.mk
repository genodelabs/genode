#
# Specifics for Fiasco.OC on x86 64-bit
#

SPECS += x86_64 foc
SPECS += pci ps2 vesa framebuffer

#
# L4/sys headers
#
L4_INC_DIR  = $(BUILD_BASE_DIR)/include/amd64
L4F_INC_DIR = $(BUILD_BASE_DIR)/include/amd64/l4f

#
# Compile for 64-bit
#
CC_OPT += -m64

#
# Also include less-specific configuration last
#
include $(call select_from_repositories,mk/spec/x86_64.mk)
include $(call select_from_repositories,mk/spec/foc.mk)

INC_DIR += $(L4F_INC_DIR) $(L4_INC_DIR)
