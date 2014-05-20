SRC_C   += usbnet.c smsc95xx.c

include $(REP_DIR)/lib/mk/usb.inc
include $(REP_DIR)/lib/mk/armv7/usb.inc

CC_OPT  += -DCONFIG_USB_EHCI_HCD_OMAP -DCONFIG_USB_EHCI_TT_NEWSCHED -DVERBOSE_DEBUG
SRC_CC  += platform.cc

vpath platform.cc $(LIB_DIR)/arm/platform_panda
vpath %.c         $(LX_CONTRIB_DIR)/drivers/net/usb
