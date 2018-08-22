include $(REP_DIR)/src/drivers/usb_host/target.inc

TARGET    = wand_quad_usb_host_drv
REQUIRES  = wand_quad

SRC_C    += usb/chipidea/ci_hdrc_imx.c
SRC_C    += usb/chipidea/core.c
SRC_C    += usb/chipidea/host.c
SRC_C    += usb/chipidea/otg.c
SRC_C    += usb/chipidea/usbmisc_imx.c
SRC_C    += usb/phy/phy-mxs-usb.c

INC_DIR += $(REP_DIR)/src/drivers/usb_host/spec/arm
INC_DIR += $(REP_DIR)/src/include/spec/arm

SRC_CC  += spec/arm/platform.cc
SRC_CC   += spec/wand_quad/platform.cc

CC_OPT   += -DCONFIG_USB_CHIPIDEA
CC_OPT   += -DCONFIG_USB_CHIPIDEA_HOST
CC_OPT   += -DCONFIG_USB_EHCI_HCD
CC_OPT   += -DCONFIG_USB_EHCI_ROOT_HUB_TT
CC_OPT   += -DCONFIG_USB_MXS_PHY
CC_OPT   += -DCONFIG_USB_EHCI_TT_NEWSCHED=1
CC_OPT   += -DCONFIG_EXTCON=1

CC_C_OPT += -Wno-incompatible-pointer-types
