TARGET  := usb_hid_legacy_drv
SRC_C   := dummies.c
SRC_CC  := main.cc lx_emul.cc evdev.cc
SRC_CC  += printf.cc bug.cc timer.cc scheduler.cc env.cc work.cc

LIBS    := base usb_hid_include lx_kit_setjmp format

USB_CONTRIB_DIR := $(call select_from_ports,dde_linux)/src/drivers/usb_hid

INC_DIR += $(USB_CONTRIB_DIR)/drivers/usb/core
INC_DIR += $(PRG_DIR)
INC_DIR += $(REP_DIR)/src/include

SRC_C += drivers/hid/hid-apple.c
SRC_C += drivers/hid/hid-cherry.c
SRC_C += drivers/hid/hid-core.c
SRC_C += drivers/hid/hid-generic.c
SRC_C += drivers/hid/hid-holtek-mouse.c
SRC_C += drivers/hid/hid-input.c
SRC_C += drivers/hid/hid-microsoft.c
SRC_C += drivers/hid/hid-multitouch.c
SRC_C += drivers/hid/hid-quirks.c
SRC_C += drivers/hid/wacom_sys.c
SRC_C += drivers/hid/wacom_wac.c
SRC_C += drivers/hid/usbhid/hid-core.c
SRC_C += drivers/input/evdev.c
SRC_C += drivers/input/input-mt.c
SRC_C += drivers/input/input.c
SRC_C += drivers/usb/core/config.c
SRC_C += drivers/usb/core/generic.c
SRC_C += drivers/usb/core/quirks.c

CC_OPT   += -D__KERNEL__
CC_C_OPT += -Wno-unused-but-set-variable -Wno-pointer-sign \
            -Wno-incompatible-pointer-types -Wno-unused-variable \
            -Wno-unused-function -Wno-uninitialized -Wno-maybe-uninitialized

CC_CXX_WARN_STRICT =

vpath %.c  $(USB_CONTRIB_DIR)
vpath %.cc $(REP_DIR)/src/lib/legacy/lx_kit
