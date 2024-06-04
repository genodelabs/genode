TARGET = wireguard

LIBS += base net jitterentropy wireguard

DDE_LINUX_DIR := $(subst /src/include/lx_kit,,$(call select_from_repositories,src/include/lx_kit))

MUSL_TM_DIR = $(call select_from_repositories,src/lib/musl_tm)

INC_DIR += $(PRG_DIR)/../..
INC_DIR += $(DDE_LINUX_DIR)/src/include/virt_linux
INC_DIR += $(DDE_LINUX_DIR)/src/include
INC_DIR += $(MUSL_TM_DIR)

SRC_C += tm_to_secs.c

SRC_CC += arp_cache.cc
SRC_CC += arp_waiter.cc
SRC_CC += base64.cc
SRC_CC += config_model.cc
SRC_CC += dhcp_client.cc
SRC_CC += ipv4_address_prefix.cc
SRC_CC += ipv4_config.cc
SRC_CC += lx_emul/random.cc
SRC_CC += main.cc
SRC_CC += nic_connection.cc
SRC_CC += uplink_connection.cc

vpath tm_to_secs.c $(MUSL_TM_DIR)
vpath %.cc $(REP_DIR)/src/app/wireguard
vpath %.cc $(DDE_LINUX_DIR)/src/lib

CONFIG_XSD := ../../config.xsd
