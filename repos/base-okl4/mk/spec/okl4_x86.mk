#
# Specifics for OKL4 on x86
#

SPECS += x86_32 okl4
SPECS += pci ps2 vesa framebuffer

#
# Also include less-specific configuration last
#
include $(call select_from_repositories,mk/spec/x86_32.mk)
include $(call select_from_repositories,mk/spec/okl4.mk)
