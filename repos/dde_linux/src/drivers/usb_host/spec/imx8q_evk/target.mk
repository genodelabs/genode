include $(REP_DIR)/src/drivers/usb_host/target.inc

TARGET    = imx8q_evk_usb_host_drv
REQUIRES  = arm_v8

SRC_C   += usb/dwc3/core.c
SRC_C   += usb/dwc3/host.c
SRC_C   += usb/host/xhci-plat.c

INC_DIR += $(REP_DIR)/src/drivers/usb_host/spec/arm
INC_DIR += $(REP_DIR)/src/include/spec/arm_64

SRC_CC  += spec/arm/platform.cc
SRC_CC  += spec/imx8q_evk/platform.cc

CC_OPT  += -DCONFIG_ARM64
CC_OPT  += -DCONFIG_USB_DWC3_HOST=1
