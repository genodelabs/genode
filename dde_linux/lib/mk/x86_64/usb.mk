SRC_C   += $(addprefix usb/host/,pci-quirks.c uhci-hcd.c ehci-pci.c)

include $(REP_DIR)/lib/mk/usb.inc

CC_OPT  += -DCONFIG_PCI -DCONFIG_USB_EHCI_PCI=1 -DCONFIG_USB_XHCI_HCD=0
INC_DIR += $(LIB_INC_DIR)/x86_64 $(LIB_INC_DIR)/x86
SRC_CC  += pci_driver.cc platform.cc

vpath platform.cc $(LIB_DIR)/x86
