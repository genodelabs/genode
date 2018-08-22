include $(REP_DIR)/src/drivers/usb_host/target.inc

TARGET   = x86_pc_usb_host_drv

INC_DIR += $(REP_DIR)/src_drivers/usb_host/spec/x86
INC_DIR += $(REP_DIR)/src/include/spec/x86

SRC_C   += usb/core/hcd-pci.c
SRC_C   += usb/host/ehci-pci.c
SRC_C   += usb/host/ohci-hcd.c
SRC_C   += usb/host/ohci-pci.c
SRC_C   += usb/host/pci-quirks.c
SRC_C   += usb/host/uhci-hcd.c
SRC_C   += usb/host/xhci-pci.c

SRC_CC  += lx_kit/mapped_io_mem_range.cc
SRC_CC  += lx_kit/pci.cc

SRC_CC  += spec/x86/platform.cc

CC_OPT  += -DCONFIG_PCI=1
CC_OPT  += -DCONFIG_USB_PCI=1
CC_OPT  += -DCONFIG_USB_EHCI_PCI=1
CC_OPT  += -DCONFIG_USB_XHCI_HCD=1
