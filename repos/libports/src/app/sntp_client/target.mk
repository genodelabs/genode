TARGET = sntp_client

LIBS += base net

SRC_CC += main.cc dhcp_client.cc ipv4_address_prefix.cc nic.cc ipv4_config.cc

NIC_ROUTER_DIR = $(call select_from_repositories,src/server/nic_router)

INC_DIR += $(PRG_DIR) $(NIC_ROUTER_DIR)

vpath ipv4_address_prefix.cc $(NIC_ROUTER_DIR)

CONFIG_XSD = config.xsd

# musl-libc contrib sources
MUSL_TM  = $(REP_DIR)/src/lib/musl_tm
SRC_C    = secs_to_tm.c tm_to_secs.c
INC_DIR += $(MUSL_TM)

vpath %.c $(MUSL_TM)

CC_CXX_WARN_STRICT_CONVERSION =
