LIB_DIR      = $(REP_DIR)/src/lib/qemu-usb
QEMU_USB_DIR = $(call select_from_ports,qemu-usb)/src/lib/qemu/hw/usb

CC_WARN=

INC_DIR += $(LIB_DIR) $(QEMU_USB_DIR)

LIBS  = qemu-usb_include
LIBS += libc
LIBS += libyuv

SRC_CC = dummies.cc qemu_emul.cc host.cc webcam.cc

SRC_C  = desc.c hcd-xhci.c hcd-xhci-pci.c core.c bus.c
SRC_C += dev-webcam.c

SHARED_LIB = yes

LD_OPT += --version-script=$(LIB_DIR)/symbol.map

vpath dev-webcam.c $(LIB_DIR)
vpath %.c  $(QEMU_USB_DIR)
vpath %.cc $(LIB_DIR)

CC_CXX_WARN_STRICT =
