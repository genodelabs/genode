TARGET   = ahci_drv
SRC_CC   = main.cc ahci.cc
INC_DIR += $(PRG_DIR)
LIBS    += base ahci_platform
