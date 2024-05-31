TARGET     = virtio_pci_nic
REQUIRES   = x86
SRC_CC     = pci_device.cc
LIBS       = base nic_driver
INC_DIR    = $(REP_DIR)/src/driver/nic/virtio
CONFIG_XSD = ../../config.xsd

vpath % $(REP_DIR)/src/driver/nic/virtio
