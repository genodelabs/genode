TARGET = nic_dump

LIBS += base net

SRC_CC += component.cc main.cc packet_log.cc uplink.cc interface.cc

INC_DIR += $(PRG_DIR)
