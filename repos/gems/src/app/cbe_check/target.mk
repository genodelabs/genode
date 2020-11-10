REQUIRES += x86_64

TARGET  := cbe_check

SRC_CC  += main.cc
INC_DIR += $(PRG_DIR)
LIBS    += base cbe_check_cxx

CONFIG_XSD = config.xsd
