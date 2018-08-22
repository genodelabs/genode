LX_CONTRIB_DIR    := $(call select_from_ports,dde_linux)/src/drivers/usb_host
SRC_CC             = main.cc lx_emul.cc raw.cc
SRC_C              = dummies.c raw_driver.c
LIBS               = base usb_host_include lx_kit_setjmp
CC_CXX_WARN_STRICT =

INC_DIR  += $(REP_DIR)/src/drivers/usb_host
INC_DIR  += $(REP_DIR)/src/include

# lx_kit
SRC_CC   += lx_kit/printf.cc
SRC_CC   += lx_kit/work.cc
SRC_CC   += lx_kit/timer.cc
SRC_CC   += lx_kit/scheduler.cc
SRC_CC   += lx_kit/irq.cc
SRC_CC   += lx_kit/malloc.cc
SRC_CC   += lx_kit/env.cc

# contrib code
SRC_C    += usb/common/common.c
SRC_C    += usb/core/buffer.c
SRC_C    += usb/core/config.c
SRC_C    += usb/core/devices.c
SRC_C    += usb/core/driver.c
SRC_C    += usb/core/endpoint.c
SRC_C    += usb/core/file.c
SRC_C    += usb/core/generic.c
SRC_C    += usb/core/hcd.c
SRC_C    += usb/core/hub.c
SRC_C    += usb/core/message.c
SRC_C    += usb/core/notify.c
SRC_C    += usb/core/port.c
SRC_C    += usb/core/quirks.c
SRC_C    += usb/core/urb.c
SRC_C    += usb/core/usb.c
SRC_C    += usb/host/ehci-hcd.c
SRC_C    += usb/host/xhci-dbg.c
SRC_C    += usb/host/xhci-hub.c
SRC_C    += usb/host/xhci-mem.c
SRC_C    += usb/host/xhci-ring.c
SRC_C    += usb/host/xhci.c
SRC_C    += lib/ctype.c
SRC_C    += lib/find_bit.c

CC_OPT   += -U__linux__
CC_OPT   += -D__KERNEL__
CC_OPT   += -DCONFIG_USB_DEVICEFS=1
CC_OPT   += -DCONFIG_HOTPLUG=1
CC_OPT   += -DCONFIG_USB_PHY=1
CC_OPT   += -DCONFIG_HAS_DMA=1
CC_C_OPT += -std=gnu89
CC_C_OPT += -Wno-pointer-sign
CC_C_OPT += -Wno-unused-variable
CC_C_OPT += -Wno-unused-function
CC_C_OPT += -Wno-implicit-int
CC_C_OPT += -Wno-unused-but-set-variable
CC_C_OPT += -Wno-uninitialized
CC_C_OPT += -Wno-maybe-uninitialized
CC_C_OPT += -Wno-unused-label
CC_C_OPT += -Wno-format

vpath lib/ctype.c    $(LX_CONTRIB_DIR)
vpath lib/find_bit.c $(LX_CONTRIB_DIR)
vpath %.c            $(LX_CONTRIB_DIR)/drivers
vpath %.cc           $(REP_DIR)/src
vpath %              $(REP_DIR)/src/drivers/usb_host
