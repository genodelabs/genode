#
# Specifics for the NOVA kernel API
#

SPECS += nova
SPECS += pci ps2 vesa framebuffer

#
# Linker options that are specific for x86
#
LD_TEXT_ADDR ?= 0x01000000
