TARGET = nic_router

LIBS += base net

SRC_CC += \
	arp_waiter.cc \
	ipv4_address_prefix.cc \
	nic_session_root.cc \
	port_allocator.cc \
	forward_rule.cc \
	nat_rule.cc \
	main.cc \
	ipv4_config.cc \
	nic_client.cc \
	interface.cc \
	arp_cache.cc \
	configuration.cc \
	domain.cc \
	l3_protocol.cc \
	link.cc \
	transport_rule.cc \
	permit_rule.cc \
	dns.cc \
	dhcp_client.cc \
	dhcp_server.cc \
	report.cc \
	xml_node.cc \
	uplink_session_root.cc \
	communication_buffer.cc \

INC_DIR += $(PRG_DIR)

CONFIG_XSD = config.xsd

CC_CXX_WARN_STRICT_CONVERSION =
