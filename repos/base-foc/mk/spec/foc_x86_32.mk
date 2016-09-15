#
# Specifics for Fiasco.OC on x86
#

SPECS += x86_32 foc
SPECS += pci ps2 vesa framebuffer

#
# L4/sys headers
#
L4_INC_DIR  = $(BUILD_BASE_DIR)/include/x86
L4F_INC_DIR = $(BUILD_BASE_DIR)/include/x86/l4f

#
# Also include less-specific configuration last
#
include $(call select_from_repositories,mk/spec/x86_32.mk)
include $(call select_from_repositories,mk/spec/foc.mk)

INC_DIR += $(L4F_INC_DIR) $(L4_INC_DIR)
