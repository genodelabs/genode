TARGET   = usb_drv
REQUIRES = x86 32bit
LIBS     = cxx env dde_kit server libc-setjmp signal
SRC_CC   = main.cc lx_emul.cc pci_driver.cc irq.cc timer.cc event.cc storage.cc \
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
INC_DIR += $(shell pwd)

CC_OPT += -U__linux__ -D__KERNEL__
CC_OPT += -DCONFIG_USB_DEVICEFS -DCONFIG_HOTPLUG -DCONFIG_PCI -DDEBUG

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
#SRC_C += $(addprefix usb/host/,uhci-hcd.c pci-quirks.c ehci-hcd.c ohci-hcd.c)
SRC_C += $(addprefix usb/host/,pci-quirks.c uhci-hcd.c ehci-hcd.c)

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
NO_GEN_INCLUDES := ../../scsi/sd.h

#
# Filter out original Linux headers that exist in the contrib directory
#
NO_GEN_INCLUDES := $(shell cd $(CONTRIB_DIR)/include; find -name "*.h" | sed "s/.\///")
GEN_INCLUDES := $(filter-out $(NO_GEN_INCLUDES),$(GEN_INCLUDES))

#
# Make sure to create the header symlinks prior building
#
$(SRC_C:.c=.o) $(SRC_CC:.cc=.o): $(GEN_INCLUDES)

#
# Add prefix, since there are two hid-core.c with the same module init function
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
