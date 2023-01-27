LIBS += base net jitterentropy wireguard_lx_inc_dirs

INC_DIR += $(REP_DIR)/src/app/wireguard
INC_DIR += $(REP_DIR)/src/include/virt_linux
INC_DIR += $(REP_DIR)/src/include

SRC_CC += arp_cache.cc
SRC_CC += arp_waiter.cc
SRC_CC += base64.cc
SRC_CC += config_model.cc
SRC_CC += dhcp_client.cc
SRC_CC += ipv4_address_prefix.cc
SRC_CC += ipv4_config.cc
SRC_CC += lx_emul/random.cc
SRC_CC += main.cc
SRC_CC += nic_connection.cc
SRC_CC += uplink_connection.cc

vpath %.cc $(REP_DIR)/src/app/wireguard
vpath %.cc $(REP_DIR)/src/lib
