include $(REP_DIR)/src/drivers/usb_host/target.inc

TARGET   = panda_usb_host_drv
REQUIRES = panda

INC_DIR += $(REP_DIR)/src/drivers/usb_host/spec/arm
INC_DIR += $(REP_DIR)/src/include/spec/arm

SRC_CC  += spec/arm/platform.cc
SRC_CC  += spec/panda/platform.cc
SRC_C   += usb/host/ehci-omap.c

CC_OPT  += -DCONFIG_USB_EHCI_HCD_OMAP=1
CC_OPT  += -DCONFIG_USB_EHCI_TT_NEWSCHED=1
CC_OPT  += -DCONFIG_EXTCON=1
