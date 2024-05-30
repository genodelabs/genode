TARGET     = virtio_mmio_input
SRC_CC     = mmio_device.cc
LIBS       = base
INC_DIR    = $(REP_DIR)/src/drivers/input/virtio

vpath % $(REP_DIR)/src/drivers/input/virtio
