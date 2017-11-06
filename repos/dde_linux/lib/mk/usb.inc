LIB_DIR     = $(REP_DIR)/src/lib/usb
LIB_INC_DIR = $(LIB_DIR)/include

LIBS     += usb_include lx_kit_setjmp
SRC_CC   += main.cc lx_emul.cc storage.cc \
            input_component.cc evdev.cc nic.cc raw.cc
SRC_C    += dummies.c scsi.c raw_driver.c

LX_CONTRIB_DIR := $(call select_from_ports,dde_linux)/src/lib/usb
DRIVERS_DIR    := $(LX_CONTRIB_DIR)/drivers
USB_DIR        := $(DRIVERS_DIR)/usb

#
# The order of include-search directories is important, we need to look into
# 'contrib' before falling back to our custom 'lx_emul.h' header.
#
INC_DIR += $(LIB_INC_DIR) $(REP_DIR)/src/include
INC_DIR += $(LX_CONTRIB_DIR)/include $(LX_CONTRIB_DIR)/include/uapi $(LX_CONTRIB_DIR)

CC_OPT += -U__linux__ -D__KERNEL__
CC_OPT += -DCONFIG_USB_DEVICEFS -DCONFIG_HOTPLUG -DDEBUG -DCONFIG_USB_PHY=1 \
          -DCONFIG_GENERIC_PHY=0 -DCONFIG_USB_OTG_WHITELIST=0 \
          -DCONFIG_USB_OTG=0 -DCONFIG_USB_NET_RNDIS_HOST=1

CC_WARN = -Wall -Wno-unused-variable -Wno-uninitialized \
          -Wno-unused-function -Wno-overflow

CC_C_OPT += -std=gnu89 -Wno-unused-but-set-variable -Wno-pointer-sign -Wno-unused-label
CC_C_OPT += -include $(LIB_INC_DIR)/lx_emul.h

CC_CXX_OPT = -fpermissive

#
# Suffix of global 'module_init' function
#
MOD_SUFFIX =
CC_OPT += -DMOD_SUFFIX=$(MOD_SUFFIX)

# lx_kit
SRC_CC += printf.cc work.cc timer.cc scheduler.cc irq.cc malloc.cc env.cc

# common lib
SRC_C += lib/ctype.c
SRC_C += lib/int_sqrt.c

# USB core
SRC_C += $(addprefix usb/core/, buffer.c config.c devices.c driver.c endpoint.c file.c \
                                generic.c hcd.c hub.c message.c notify.c port.c quirks.c\
                                urb.c usb.c)

# USB host-controller driver
SRC_C += $(addprefix usb/host/, ehci-hcd.c)

# common
SRC_C += usb/common/common.c

# USB hid
SRC_C += $(addprefix hid/usbhid/, hid-core.c hid-quirks.c)
SRC_C += $(addprefix hid/,        hid-core.c hid-generic.c hid-input.c \
                                  hid-cherry.c hid-microsoft.c hid-multitouch.c \
                                  wacom_sys.c wacom_wac.c)
SRC_C += $(addprefix input/,      evdev.c input.c input-mt.c)

# USB storage
SRC_C += $(addprefix usb/storage/,scsiglue.c protocol.c transport.c usb.c \
           initializers.c option_ms.c sierra_ms.c usual-tables.c)

# SCSI
SRC_C += $(addprefix scsi/,scsi.c constants.c)

#SRC_CC = storage.cc
#SRC_C =

#
# Add suffix, since there are two hid-core.c with the same module init function
#
hid/hid-core.o: MOD_SUFFIX="_core"

vpath %.c  $(DRIVERS_DIR)
vpath %.c  $(LIB_DIR)
vpath %.cc $(LIB_DIR)
vpath %.cc $(LIB_DIR)/signal
vpath %.c  $(LIB_DIR)/input
vpath %.cc $(LIB_DIR)/input
vpath %.cc $(LIB_DIR)/storage
vpath %.c  $(LIB_DIR)/storage
vpath %.cc $(LIB_DIR)/nic
vpath %.cc $(LIB_DIR)/raw
vpath %.c  $(LIB_DIR)/raw
vpath lib/int_sqrt.c $(LX_CONTRIB_DIR)
vpath lib/ctype.c $(LX_CONTRIB_DIR)
vpath %.cc $(REP_DIR)/src/lx_kit

# vi: set ft=make :
