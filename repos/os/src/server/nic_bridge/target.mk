TARGET   = nic_bridge
LIBS     = base net
SRC_CC   = component.cc mac_allocator.cc main.cc nic.cc packet_handler.cc
INC_DIR += $(PRG_DIR)
