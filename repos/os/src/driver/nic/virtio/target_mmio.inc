TARGET     = virtio_mmio_nic
SRC_CC     = mmio_device.cc
LIBS       = base nic_driver
INC_DIR    = $(REP_DIR)/src/driver/nic/virtio
CONFIG_XSD = ../../config.xsd

vpath % $(REP_DIR)/src/driver/nic/virtio
