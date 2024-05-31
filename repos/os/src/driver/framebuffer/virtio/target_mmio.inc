TARGET     = virtio_mmio_fb
SRC_CC     = mmio_device.cc
LIBS       = base blit
INC_DIR    = $(REP_DIR)/src/driver/framebuffer/virtio

vpath % $(REP_DIR)/src/driver/framebuffer/virtio
