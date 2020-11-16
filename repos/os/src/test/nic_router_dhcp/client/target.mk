TARGET = test-nic_router_dhcp-client

LIBS += base net

SRC_CC += main.cc dhcp_client.cc ipv4_address_prefix.cc
SRC_CC += nic.cc ipv4_config.cc

INC_DIR += $(PRG_DIR)
