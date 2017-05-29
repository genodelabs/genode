TARGET = nic_router

LIBS += base net

SRC_CC += arp_waiter.cc ip_rule.cc
SRC_CC += component.cc port_allocator.cc forward_rule.cc
SRC_CC += nat_rule.cc mac_allocator.cc main.cc
SRC_CC += uplink.cc interface.cc arp_cache.cc configuration.cc
SRC_CC += domain.cc protocol_name.cc direct_rule.cc link.cc
SRC_CC += transport_rule.cc leaf_rule.cc permit_rule.cc

INC_DIR += $(PRG_DIR)
