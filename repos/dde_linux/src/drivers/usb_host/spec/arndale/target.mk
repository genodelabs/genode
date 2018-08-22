include $(REP_DIR)/src/drivers/usb_host/target.inc

TARGET   = arndale_usb_host_drv
REQUIRES = arndale

INC_DIR += $(REP_DIR)/src/drivers/usb_host/spec/arm
INC_DIR += $(REP_DIR)/src/include/spec/arm

SRC_CC  += spec/arm/platform.cc
SRC_CC  += spec/arndale/platform.cc

SRC_C   += usb/dwc3/core.c
SRC_C   += usb/dwc3/dwc3-exynos.c
SRC_C   += usb/dwc3/host.c
SRC_C   += usb/host/ehci-exynos.c
SRC_C   += usb/host/xhci-plat.c

CC_OPT  += -DCONFIG_USB_EHCI_TT_NEWSCHED=1
CC_OPT  += -DCONFIG_USB_DWC3_HOST=1
CC_OPT  += -DCONFIG_USB_OTG_UTILS=1
CC_OPT  += -DCONFIG_USB_XHCI_PLATFORM=1
CC_OPT  += -DDWC3_QUIRK
