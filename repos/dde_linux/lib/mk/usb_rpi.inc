SRC_C += \
	usb/host/dwc_otg/dwc_otg/dwc_otg_adp.c \
	usb/host/dwc_otg/dwc_otg/dwc_otg_attr.c \
	usb/host/dwc_otg/dwc_otg/dwc_otg_cfi.c \
	usb/host/dwc_otg/dwc_otg/dwc_otg_cil.c \
	usb/host/dwc_otg/dwc_otg/dwc_otg_cil_intr.c \
	usb/host/dwc_otg/dwc_otg/dwc_otg_driver.c \
	usb/host/dwc_otg/dwc_otg/dwc_otg_hcd.c \
	usb/host/dwc_otg/dwc_otg/dwc_otg_hcd_ddma.c \
	usb/host/dwc_otg/dwc_otg/dwc_otg_hcd_intr.c \
	usb/host/dwc_otg/dwc_otg/dwc_otg_hcd_linux.c \
	usb/host/dwc_otg/dwc_otg/dwc_otg_hcd_queue.c

SRC_C += \
	usb/host/dwc_otg/dwc_common_port/dwc_cc.c \
	usb/host/dwc_otg/dwc_common_port/dwc_common_linux.c \
	usb/host/dwc_otg/dwc_common_port/dwc_crypto.c \
	usb/host/dwc_otg/dwc_common_port/dwc_dh.c \
	usb/host/dwc_otg/dwc_common_port/dwc_mem.c \
	usb/host/dwc_otg/dwc_common_port/dwc_modpow.c \
	usb/host/dwc_otg/dwc_common_port/dwc_notifier.c

SRC_C += net/usb/usbnet.c net/usb/smsc95xx.c

include $(REP_DIR)/lib/mk/usb.inc
include $(REP_DIR)/lib/mk/armv6/usb.inc

CC_OPT  += -DDWC_LINUX -DPLATFORM_INTERFACE

# needed for 'ehci-hcd.c', which we don't use on the rpi, but it is still
# part of the generic usb USB driver
CC_OPT += -DCONFIG_USB_EHCI_PCI=1

# for 'dwc_otg_hcd_linux.c' for enabling the FIQ, which we don't use anyway
CC_OPT  += -DINTERRUPT_VC_USB=9

# for 'dwc_otg_driver.c' for preventing calls to set_irq_type
CC_OPT  += -DIRQF_TRIGGER_LOW=1

INC_DIR += $(LX_CONTRIB_DIR)/drivers/usb/host/dwc_otg/dwc_common_port \
           $(LX_CONTRIB_DIR)/drivers/usb/host/dwc_otg/dwc_otg
SRC_CC  += platform.cc

vpath platform.cc $(LIB_DIR)/arm/platform_rpi
vpath %.c         $(LX_CONTRIB_DIR)/drivers/net/usb

# enable C++11 support
CC_CXX_OPT += -std=gnu++11
