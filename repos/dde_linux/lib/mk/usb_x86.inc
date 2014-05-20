SRC_C   += $(addprefix usb/host/,pci-quirks.c uhci-hcd.c ehci-pci.c)

#
# USB netwpork support
#
SRC_C   += usbnet.c ax88179_178a.c

#XHCI
SRC_C += xhci-pci.c

SRC_CC  += pci_driver.cc platform.cc

include $(REP_DIR)/lib/mk/xhci.inc
include $(REP_DIR)/lib/mk/usb.inc

CC_OPT  += -DCONFIG_PCI -DCONFIG_USB_EHCI_PCI=1 -DCONFIG_USB_XHCI_HCD=1

vpath platform.cc $(LIB_DIR)/x86
vpath %.c         $(LX_CONTRIB_DIR)/drivers/net/usb

# vi:set ft=make :
