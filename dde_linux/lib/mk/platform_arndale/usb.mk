SRC_C   += usbnet.c asix.c

include $(REP_DIR)/lib/mk/usb.inc
include $(REP_DIR)/lib/mk/arm/usb.inc

CC_OPT  += -DCONFIG_USB_EHCI_S5P -DCONFIG_USB_EHCI_TT_NEWSCHED
INC_DIR += $(CONTRIB_DIR)/arch/arm/plat-samsung/include
SRC_CC  += platform.cc

vpath platform.cc $(LIB_DIR)/arm/platform_arndale
vpath %.c         $(CONTRIB_DIR)/drivers/net/usb 
