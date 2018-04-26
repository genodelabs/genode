TARGET = ping

LIBS += base net

SRC_CC += main.cc dhcp_client.cc xml_node.cc ipv4_address_prefix.cc
SRC_CC += nic.cc ipv4_config.cc

INC_DIR += $(PRG_DIR)

CONFIG_XSD = config.xsd
