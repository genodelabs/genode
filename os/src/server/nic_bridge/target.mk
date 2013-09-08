TARGET    = nic_bridge
LIBS      = base net config
SRC_CC    = component.cc env.cc mac.cc main.cc nic.cc packet_handler.cc

vpath *.cc $(REP_DIR)/src/server/proxy_arp
