TARGET   = platform_drv
REQUIRES = x86
SRC_CC   = main.cc irq.cc pci_device.cc nonpci_devices.cc session.cc
LIBS     = base

INC_DIR  = $(PRG_DIR)
