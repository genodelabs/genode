REQUIRES += x86_64

TARGET  := tresor_check

SRC_CC  += main.cc
INC_DIR += $(PRG_DIR)
LIBS    += base tresor_check_cxx

CONFIG_XSD = config.xsd
