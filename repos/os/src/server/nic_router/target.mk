TARGET = nic_router

LIBS += base net

SRC_CC += arp_waiter.cc ip_rule.cc ipv4_address_prefix.cc
SRC_CC += component.cc port_allocator.cc forward_rule.cc
SRC_CC += nat_rule.cc main.cc ipv4_config.cc
SRC_CC += uplink.cc interface.cc arp_cache.cc configuration.cc
SRC_CC += domain.cc l3_protocol.cc direct_rule.cc link.cc
SRC_CC += transport_rule.cc permit_rule.cc
SRC_CC += dhcp_client.cc dhcp_server.cc report.cc xml_node.cc

INC_DIR += $(PRG_DIR)

CONFIG_XSD = config.xsd
