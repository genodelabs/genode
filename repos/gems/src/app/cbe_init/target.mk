REQUIRES += x86_64

TARGET  := cbe_init

SRC_CC  += main.cc
INC_DIR += $(PRG_DIR)
LIBS    += base vfs cbe_init_cxx

CONFIG_XSD = config.xsd
