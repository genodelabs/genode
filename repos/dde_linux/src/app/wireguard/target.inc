TARGET    = wireguard
LIBS     += base net jitterentropy wireguard
GEN_DIR  := $(PRG_DIR)/../..
INC_DIR  += $(GEN_DIR)
INC_DIR  += $(REP_DIR)/src/include
SRC_CC   += arp_cache.cc
SRC_CC   += arp_waiter.cc
SRC_CC   += base64.cc
SRC_CC   += config_model.cc
SRC_CC   += dhcp_client.cc
SRC_CC   += ipv4_address_prefix.cc
SRC_CC   += ipv4_config.cc
SRC_CC   += irq.cc
SRC_CC   += main.cc
SRC_CC   += nic_connection.cc
SRC_CC   += random.cc
SRC_CC   += uplink_connection.cc

vpath %.cc $(GEN_DIR)

CC_CXX_WARN_STRICT_CONVERSION =
