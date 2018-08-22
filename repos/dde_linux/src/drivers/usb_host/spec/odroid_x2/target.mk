include $(REP_DIR)/src/drivers/usb_host/target.inc

TARGET   = odroid_x2_usb_host_drv
REQUIRES = odroid_x2

INC_DIR += $(REP_DIR)/src/drivers/usb_host/spec/arm
INC_DIR += $(REP_DIR)/src/include/spec/arm

SRC_CC  += spec/arm/platform.cc
SRC_CC  += spec/odroid_x2/platform.cc

SRC_C   += usb/host/ehci-exynos.c

CC_OPT  += -DCONFIG_USB_OTG_UTILS=1
