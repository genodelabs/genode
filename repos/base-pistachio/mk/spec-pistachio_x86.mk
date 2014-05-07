#
# Specifics for Pistachio on 32-bit x86
#

SPECS += x86_32 pistachio
SPECS += pci ps2 vesa framebuffer

#
# Linker options that are specific for x86
#
LD_TEXT_ADDR ?= 0x00300000

include $(call select_from_repositories,mk/spec-x86_32.mk)
include $(call select_from_repositories,mk/spec-pistachio.mk)
