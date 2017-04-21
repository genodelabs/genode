TARGET = nic_dump

LIBS += base net config

SRC_CC += component.cc main.cc uplink.cc interface.cc

INC_DIR += $(PRG_DIR)
