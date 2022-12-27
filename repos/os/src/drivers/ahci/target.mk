TARGET   = ahci_drv
SRC_CC  += main.cc
INC_DIR += $(PRG_DIR)
LIBS    += base

CC_CXX_WARN_STRICT_CONVERSION =

vpath %.cc $(PRG_DIR)
