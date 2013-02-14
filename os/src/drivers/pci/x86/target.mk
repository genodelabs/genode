TARGET   = pci_drv
REQUIRES = x86
SRC_CC   = main.cc
LIBS     = base

INC_DIR  = $(PRG_DIR)/..

vpath main.cc $(PRG_DIR)/..
