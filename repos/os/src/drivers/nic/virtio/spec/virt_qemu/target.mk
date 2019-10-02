TARGET    = virtio_mmio_nic
REQUIRES  = virtio_mmio virtio
SRC_CC    = main.cc
LIBS      = base
INC_DIR   = $(call select_from_repositories,include/spec/virtio_mmio)
INC_DIR  += $(REP_DIR)/src/drivers/nic/virtio

CONFIG_XSD = ../../config.xsd
