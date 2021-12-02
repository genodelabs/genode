TARGET = test-nic_router_dhcp-manager

LIBS += base

SRC_CC += main.cc ipv4_address_prefix.cc dns.cc xml_node.cc

INC_DIR += $(PRG_DIR) $(REP_DIR)/src/server/nic_router

vpath dns.cc      $(REP_DIR)/src/server/nic_router
vpath xml_node.cc $(REP_DIR)/src/server/nic_router

CC_CXX_WARN_STRICT_CONVERSION =
