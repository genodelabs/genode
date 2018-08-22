include $(REP_DIR)/src/drivers/usb_host/target.inc

TARGET   = rpi_usb_host_drv
REQUIRES = rpi

INC_DIR += $(REP_DIR)/src/drivers/usb_host/spec/arm
INC_DIR += $(REP_DIR)/src/include/spec/arm

SRC_CC  += spec/arm/platform.cc
SRC_CC += spec/rpi/platform.cc

SRC_C += dwc_common_port/dwc_cc.c
SRC_C += dwc_common_port/dwc_common_linux.c
SRC_C += dwc_common_port/dwc_crypto.c
SRC_C += dwc_common_port/dwc_dh.c
SRC_C += dwc_common_port/dwc_mem.c
SRC_C += dwc_common_port/dwc_modpow.c
SRC_C += dwc_common_port/dwc_notifier.c
SRC_C += dwc_otg/dwc_otg_adp.c
SRC_C += dwc_otg/dwc_otg_attr.c
SRC_C += dwc_otg/dwc_otg_cfi.c
SRC_C += dwc_otg/dwc_otg_cil.c
SRC_C += dwc_otg/dwc_otg_cil_intr.c
SRC_C += dwc_otg/dwc_otg_driver.c
SRC_C += dwc_otg/dwc_otg_hcd.c
SRC_C += dwc_otg/dwc_otg_hcd_ddma.c
SRC_C += dwc_otg/dwc_otg_hcd_intr.c
SRC_C += dwc_otg/dwc_otg_hcd_linux.c
SRC_C += dwc_otg/dwc_otg_hcd_queue.c

CC_OPT  += -DDWC_LINUX -DPLATFORM_INTERFACE

# needed for 'ehci-hcd.c', which we don't use on the rpi, but it is still
# part of the generic usb USB driver
CC_OPT += -DCONFIG_USB_EHCI_PCI=1

# for 'dwc_otg_hcd_linux.c' for enabling the FIQ, which we don't use anyway
CC_OPT  += -DINTERRUPT_VC_USB=9

# for 'dwc_otg_driver.c' for preventing calls to set_irq_type
CC_OPT  += -DIRQF_TRIGGER_LOW=1

INC_DIR += $(LX_CONTRIB_DIR)/drivers/usb/host/dwc_common_port \
           $(LX_CONTRIB_DIR)/drivers/usb/host/dwc_otg \
           $(REP_DIR)/src/lib/usb_host/spec/arm

vpath %.c $(LX_CONTRIB_DIR)/drivers/usb/host

LIBS += rpi_usb
