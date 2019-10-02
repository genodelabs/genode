TARGET    = virtio_pci_nic
REQUIRES  = pci
SRC_CC    = main.cc
LIBS      = base
INC_DIR   = $(call select_from_repositories,include/spec/pci)
INC_DIR  += $(REP_DIR)/src/drivers/nic/virtio

CONFIG_XSD = ../../config.xsd

