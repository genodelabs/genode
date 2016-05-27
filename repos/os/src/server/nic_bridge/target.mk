TARGET    = nic_bridge
LIBS      = base net config server
SRC_CC    = component.cc mac.cc main.cc nic.cc packet_handler.cc
INC_DIR  += $(PRG_DIR)

vpath *.cc $(REP_DIR)/src/server/proxy_arp
