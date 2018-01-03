SRC_C   += $(addprefix net/usb/, usbnet.c smsc95xx.c)
SRC_C   += usb/host/ehci-exynos.c

include $(REP_DIR)/lib/mk/spec/arm_v7/usb.inc


CC_OPT  += -DCONFIG_USB_EHCI_TT_NEWSCHED \
           -DCONFIG_USB_OTG_UTILS

SRC_CC  += platform.cc

INC_DIR += $(LIB_INC_DIR)/spec/odroid_x2

vpath platform.cc $(LIB_DIR)/spec/odroid_x2

CC_CXX_WARN_STRICT =
