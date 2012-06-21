#
# Specifics for the NOVA kernel API
#

SPECS += nova
SPECS += pci ps2 vesa

#
# Linker options that are specific for x86
#
LD_TEXT_ADDR ?= 0x01000000

#
# Startup code to be used when building a program
#
STARTUP_LIB ?= startup
PRG_LIBS += $(STARTUP_LIB)
