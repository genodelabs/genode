#
# Specifics for the NOVA kernel API
#

LD_TEXT_ADDR ?= 0x01000000

#
# Startup code to be used when building a program
#
STARTUP_LIB ?= startup
PRG_LIBS += $(STARTUP_LIB)

#
# NOVA only runs on x86, enable x86 devices
#
SPECS += pci ps2 vesa
