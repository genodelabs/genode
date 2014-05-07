SRC_C   += usbnet.c asix_devices.c asix_common.c ax88172a.c ax88179_178a.c

include $(REP_DIR)/lib/mk/xhci.inc
include $(REP_DIR)/lib/mk/usb.inc
include $(REP_DIR)/lib/mk/arm/usb.inc

CC_OPT  += -DCONFIG_USB_EHCI_S5P -DCONFIG_USB_EHCI_TT_NEWSCHED -DCONFIG_OF -DCONFIG_USB_DWC3_HOST \
           -DCONFIG_USB_OTG_UTILS -DCONFIG_USB_XHCI_PLATFORM -DDWC3_QUIRK
INC_DIR += $(CONTRIB_DIR)/arch/arm/plat-samsung/include
SRC_CC  += platform.cc

#DWC3
SRC_C   += dwc3-exynos.c host.c core.c

#XHCI
SRC_C += xhci-plat.c

vpath platform.cc $(LIB_DIR)/arm/platform_arndale
vpath %.c         $(CONTRIB_DIR)/drivers/usb/dwc3
vpath %.c         $(CONTRIB_DIR)/drivers/net/usb
