TARGET = nic_perf
SRC_CC = main.cc interface.cc packet_generator.cc dhcp_client.cc
LIBS   = base net

INC_DIR += $(PRG_DIR)

CC_CXX_WARN_STRICT_CONVERSION =
