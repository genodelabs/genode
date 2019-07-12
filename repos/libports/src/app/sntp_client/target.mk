TARGET = sntp_client

LIBS += base net

SRC_CC += main.cc dhcp_client.cc xml_node.cc ipv4_address_prefix.cc
SRC_CC += nic.cc ipv4_config.cc

INC_DIR += $(PRG_DIR)

CONFIG_XSD = config.xsd

# musl-libc contrib sources
MUSL_TM  = $(REP_DIR)/src/lib/musl_tm
SRC_C    = secs_to_tm.c tm_to_secs.c
INC_DIR += $(MUSL_TM)

vpath %.c $(MUSL_TM)
