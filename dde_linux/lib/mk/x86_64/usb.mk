SRC_C   += $(addprefix usb/host/,pci-quirks.c uhci-hcd.c)

include $(REP_DIR)/lib/mk/usb.inc

CC_OPT  += -DCONFIG_PCI
INC_DIR += $(LIB_INC_DIR)/x86_64 $(LIB_INC_DIR)/x86
SRC_CC  += pci_driver.cc
