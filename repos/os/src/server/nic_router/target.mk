TARGET = nic_router

LIBS += base net config server

SRC_CC += arp_waiter.cc ip_route.cc proxy.cc
SRC_CC += port_route.cc component.cc
SRC_CC += mac_allocator.cc main.cc uplink.cc interface.cc arp_cache.cc

INC_DIR += $(PRG_DIR)

vpath *.cc $(REP_DIR)/src/server/proxy_arp
