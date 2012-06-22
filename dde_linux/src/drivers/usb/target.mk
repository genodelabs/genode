TARGET   = usb_drv
LIBS     = cxx env dde_kit server libc-setjmp signal
SRC_CC   = main.cc lx_emul.cc irq.cc timer.cc event.cc storage.cc \
           input_component.cc
SRC_C    = dummies.c scsi.c evdev.c

CONTRIB_DIR := $(REP_DIR)/contrib
DRIVERS_DIR := $(CONTRIB_DIR)/drivers
USB_DIR     := $(DRIVERS_DIR)/usb

#
# The order of include-search directories is important, we need to look into
# 'contrib' before falling back to our custom 'lx_emul.h' header.
#
INC_DIR += $(PRG_DIR)
INC_DIR += $(CONTRIB_DIR)/include

CC_OPT += -U__linux__ -D__KERNEL__
CC_OPT += -DCONFIG_USB_DEVICEFS -DCONFIG_HOTPLUG -DDEBUG

CC_WARN = -Wall -Wno-unused-variable -Wno-uninitialized \
          -Wno-unused-function \

CC_C_OPT += -Wno-implicit-function-declaration -Wno-unused-but-set-variable \
            -Wno-pointer-sign

#
# Suffix of global 'module_init' function
#
MOD_SUFFIX =
CC_OPT += -DMOD_SUFFIX=$(MOD_SUFFIX)

# USB core
SRC_C += $(addprefix usb/core/,$(notdir $(wildcard $(USB_DIR)/core/*.c)))
SRC_C += usb/usb-common.c

# USB host-controller driver
SRC_C += $(addprefix usb/host/, ehci-hcd.c)

# USB hid
SRC_C += $(addprefix hid/usbhid/,hid-core.c hid-quirks.c usbmouse.c usbkbd.c)
SRC_C += hid/hid-input.c hid/hid-core.c input/evdev.c input/input.c

# USB storage
SRC_C += $(addprefix usb/storage/,scsiglue.c protocol.c transport.c usb.c \
           initializers.c option_ms.c sierra_ms.c usual-tables.c)

# SCSI
SRC_C += $(addprefix scsi/,scsi.c constants.c)

#
# Determine the header files included by the contrib code. For each
# of these header files we create a symlink to 'lx_emul.h'.
#
GEN_INCLUDES := $(shell grep -rh "^\#include .*\/" $(CONTRIB_DIR) |\
                        sed "s/^\#include *[<\"]\(.*\)[>\"].*/\1/" | sort | uniq)
#
# Filter out some black-listed headers
#


########################
## Platform specifics ##
########################

SUPPORTED = x86_32 platform_panda


#
# x86_32
#
ifeq ($(filter-out $(SPECS),x86_32),)
INC_DIR += $(PRG_DIR)/x86_32
CC_OPT  += -DCONFIG_PCI
SRC_C   += $(addprefix usb/host/,pci-quirks.c uhci-hcd.c)
SRC_CC  += pci_driver.cc

#
# Panda board
#
else ifeq ($(filter-out $(SPECS),platform_panda),)
CC_OPT  += -DCONFIG_USB_EHCI_HCD_OMAP -DCONFIG_USB_EHCI_TT_NEWSCHED -DVERBOSE_DEBUG
INC_DIR += $(PRG_DIR)/arm
INC_DIR += $(CONTRIB_DIR)/arch/arm/plat-omap/include
SRC_C   += platform_device.c
SRC_CC  += platform.cc
#SRC_C   += $(CONTRIB_DIR)/arch/arm/mach-omap2/usb-host.c
#SRC_C   += $(DRIVERS_DIR)/mfd/omap-usb-host.c
vpath %.c  $(PRG_DIR)/arm/platform
vpath %.cc $(PRG_DIR)/arm/platform

#
# Unsupported
#
else
$(error Skip USB because it requires one of the following: $(SUPPORTED))
endif

#
# Filter out original Linux headers that exist in the contrib directory
#
NO_GEN_INCLUDES := $(shell cd $(CONTRIB_DIR)/include; find -name "*.h" | sed "s/.\///")
GEN_INCLUDES    := $(filter-out $(NO_GEN_INCLUDES),$(GEN_INCLUDES))

#
# Put Linux headers in 'GEN_INC' dir, since some include use "../../" paths use
# three level include hierarchy
#
GEN_INC         := $(shell pwd)/include/include/include

$(shell mkdir -p $(GEN_INC))

GEN_INCLUDES    := $(addprefix $(GEN_INC)/,$(GEN_INCLUDES))
INC_DIR         += $(GEN_INC)

#
# Make sure to create the header symlinks prior building
#
$(SRC_C:.c=.o) $(SRC_CC:.cc=.o): $(GEN_INCLUDES)


#
# Add suffix, since there are two hid-core.c with the same module init function
#
hid/hid-core.o: MOD_SUFFIX="_core"

$(GEN_INCLUDES):
	$(VERBOSE)mkdir -p $(dir $@)
	$(VERBOSE)ln -s $(REP_DIR)/src/drivers/usb/lx_emul.h $@

vpath %.c  $(DRIVERS_DIR)
vpath %.c  $(USB_DIR)/host
vpath %.cc $(PRG_DIR)/signal
vpath %.c  $(PRG_DIR)/input
vpath %.cc $(PRG_DIR)/input
vpath %.cc $(PRG_DIR)/storage
vpath %.c  $(PRG_DIR)/storage

clean cleanall:
	$(VERBOSE) rm -r include
