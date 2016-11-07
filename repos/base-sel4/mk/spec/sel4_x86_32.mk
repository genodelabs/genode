#
# Specifics for the seL4 kernel API x86 32 bit
#

SPECS += sel4 x86_32 ps2 vesa framebuffer pci

include $(call select_from_repositories,mk/spec/x86_32.mk)
include $(call select_from_repositories,mk/spec/sel4.mk)
