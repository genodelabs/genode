SRC_C   += $(addprefix net/usb/, usbnet.c smsc95xx.c)
SRC_C   += usb/host/ehci-omap.c

include $(REP_DIR)/lib/mk/spec/arm_v7/usb.inc

CC_OPT  += -DCONFIG_USB_EHCI_HCD_OMAP -DCONFIG_USB_EHCI_TT_NEWSCHED -DVERBOSE_DEBUG
SRC_CC  += platform.cc

vpath platform.cc $(LIB_DIR)/spec/panda

CC_CXX_WARN_STRICT =
