TARGET = test-nic_router_uplinks

LIBS += base

NIC_ROUTER_DIR := $(call select_from_repositories,src/server/nic_router)

SRC_CC += main.cc dns.cc xml_node.cc

INC_DIR += $(PRG_DIR) $(NIC_ROUTER_DIR)

vpath dns.cc $(NIC_ROUTER_DIR)
vpath xml_node.cc $(NIC_ROUTER_DIR)

CC_CXX_WARN_STRICT_CONVERSION =
