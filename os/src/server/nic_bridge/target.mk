TARGET    = nic_bridge
LIBS      = env cxx server net signal
SRC_CC    = address_node.cc component.cc mac.cc \
            main.cc packet_handler.cc vlan.cc

vpath *.cc $(REP_DIR)/src/server/proxy_arp