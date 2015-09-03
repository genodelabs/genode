#
# Specifics for Fiasco L4v2 on x86
#

SPECS += x86_32 fiasco
SPECS += pci ps2 vesa framebuffer

#
# x86-specific L4v2/sys headers
#
L4_INC_DIR   += $(L4_BUILD_DIR)/include/x86/l4v2 \
                $(L4_BUILD_DIR)/include/x86

#
# Linker options that are specific for x86
#
LD_TEXT_ADDR ?= 0x01000000

#
# Also include less-specific configuration last
#
include $(call select_from_repositories,mk/spec/x86_32.mk)
include $(call select_from_repositories,mk/spec/fiasco.mk)

INC_DIR      += $(L4_INC_DIR)
