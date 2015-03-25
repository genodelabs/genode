TARGET   = pci_drv
REQUIRES = x86
SRC_CC   = main.cc irq.cc
LIBS     = base config

INC_DIR  = $(PRG_DIR)/..

vpath %.cc $(PRG_DIR)/..
