TARGET = nic_uplink

LIBS += base net

INC_DIR += $(PRG_DIR)
INC_DIR += $(REP_DIR)/src/server/nic_router

SRC_CC += main.cc
SRC_CC += communication_buffer.cc

vpath main.cc $(PRG_DIR)
vpath communication_buffer.cc $(REP_DIR)/src/server/nic_router

CONFIG_XSD = config.xsd
