TARGET = test-nic_router_dhcp-manager

LIBS += base

SRC_CC += main.cc ipv4_address_prefix.cc dns_server.cc

INC_DIR += $(PRG_DIR)
