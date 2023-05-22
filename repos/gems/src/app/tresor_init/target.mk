TARGET := tresor_init

SRC_CC += main.cc

INC_DIR += $(PRG_DIR)

LIBS += base
LIBS += tresor

CONFIG_XSD := config.xsd
