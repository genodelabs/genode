LIB_DIR     = $(REP_DIR)/src/lib/usb
LIB_INC_DIR = $(LIB_DIR)/include

LIBS     += base cxx dde_kit libc-setjmp config
SRC_CC   += main.cc lx_emul.cc irq.cc timer.cc event.cc storage.cc \
            input_component.cc nic.cc
SRC_C    += dummies.c scsi.c evdev.c

LX_CONTRIB_DIR := $(call select_from_ports,dde_linux)/src/lib/dde_linux
DRIVERS_DIR    := $(LX_CONTRIB_DIR)/drivers
USB_DIR        := $(DRIVERS_DIR)/usb

CC_OLEVEL = -O2

#
# The order of include-search directories is important, we need to look into
# 'contrib' before falling back to our custom 'lx_emul.h' header.
#
INC_DIR += $(LIB_INC_DIR)
INC_DIR += $(LX_CONTRIB_DIR)/include $(LX_CONTRIB_DIR)/include/uapi $(LX_CONTRIB_DIR)

CC_OPT += -U__linux__ -D__KERNEL__
CC_OPT += -DCONFIG_USB_DEVICEFS -DCONFIG_HOTPLUG -DDEBUG -DCONFIG_USB_PHY=1

CC_WARN = -Wall -Wno-unused-variable -Wno-uninitialized \
          -Wno-unused-function -Wno-overflow

CC_C_OPT += -Wno-unused-but-set-variable -Wno-pointer-sign

CXX_OPT = -fpermissive
CC_CXX_OPT = -fpermissive

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
SRC_C += $(addprefix hid/usbhid/, hid-core.c hid-quirks.c)
SRC_C += $(addprefix hid/,        hid-core.c hid-generic.c hid-input.c hid-cherry.c hid-multitouch.c)
SRC_C += $(addprefix input/,      evdev.c input.c input-mt.c)

# USB storage
SRC_C += $(addprefix usb/storage/,scsiglue.c protocol.c transport.c usb.c \
           initializers.c option_ms.c sierra_ms.c usual-tables.c)

# SCSI
SRC_C += $(addprefix scsi/,scsi.c constants.c)


#
# Determine the header files included by the contrib code. For each
# of these header files we create a symlink to 'lx_emul.h'.
#
GEN_INCLUDES := $(shell grep -rh "^\#include .*\/" $(LX_CONTRIB_DIR) |\
                        sed "s/^\#include [^<\"]*[<\"]\([^>\"]*\)[>\"].*/\1/" | sort | uniq)

#
# Filter out original Linux headers that exist in the contrib directory
#
NO_GEN_INCLUDES := $(shell cd $(LX_CONTRIB_DIR)/include; find -name "*.h" | sed "s/.\///" | sed "s/.*include\///")
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
	$(VERBOSE)ln -s $(LIB_INC_DIR)/lx_emul.h $@

vpath %.c  $(DRIVERS_DIR)
vpath %.c  $(USB_DIR)/host
vpath %.c  $(LIB_DIR)
vpath %.cc $(LIB_DIR)
vpath %.cc $(LIB_DIR)/signal
vpath %.c  $(LIB_DIR)/input
vpath %.cc $(LIB_DIR)/input
vpath %.cc $(LIB_DIR)/storage
vpath %.c  $(LIB_DIR)/storage
vpath %.cc $(LIB_DIR)/nic

# vi: set ft=make :
