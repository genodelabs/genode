TARGET = test-net_flood

LIBS += base net

SRC_CC += main.cc dhcp_client.cc ipv4_address_prefix.cc
SRC_CC += nic.cc ipv4_config.cc

NIC_ROUTER_DIR = $(call select_from_repositories,src/server/nic_router)

INC_DIR += $(PRG_DIR) $(NIC_ROUTER_DIR)

vpath ipv4_address_prefix.cc $(NIC_ROUTER_DIR)

CONFIG_XSD = config.xsd

CC_CXX_WARN_STRICT_CONVERSION =
