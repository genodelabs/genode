SRC_C   += $(addprefix usb/host/,pci-quirks.c uhci-hcd.c ehci-pci.c ohci-hcd.c ohci-pci.c)
SRC_C   += usb/core/hcd-pci.c

#
# USB netwpork support
#
SRC_C   += $(addprefix net/usb/, usbnet.c ax88179_178a.c cdc_ether.c rndis_host.c)

# XHCI
SRC_C += usb/host/xhci-pci.c

# PCI
SRC_CC  += pci_driver.cc platform.cc

# lx_kit
SRC_CC  += pci.cc mapped_io_mem_range.cc


include $(REP_DIR)/src/drivers/usb/xhci.inc
include $(REP_DIR)/src/drivers/usb/target.inc

CC_OPT  += -DCONFIG_PCI -DCONFIG_USB_EHCI_PCI=1 -DCONFIG_USB_XHCI_HCD=1

vpath platform.cc   $(LIB_DIR)/spec/x86
vpath pci_driver.cc $(LIB_DIR)/spec/x86

# vi:set ft=make :
