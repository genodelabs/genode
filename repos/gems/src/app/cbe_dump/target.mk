REQUIRES += x86_64

TARGET  := cbe_dump

SRC_CC  += main.cc
INC_DIR += $(PRG_DIR)
LIBS    += base cbe_dump_cxx

CONFIG_XSD = config.xsd
