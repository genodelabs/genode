#
# lwIP TCP/IP library
#
# The library implements TCP and UDP as well as DNS and DHCP.
#

LWIP_PORT_DIR := $(call select_from_ports,lwip)
LWIPDIR := $(LWIP_PORT_DIR)/src/lib/lwip/src

LIBS += format

CC_C_OPT += -std=gnu11

-include $(LWIPDIR)/Filelists.mk

# Core files
SRC_C += $(notdir $(COREFILES))

# IPv4 files
SRC_C += $(notdir $(CORE4FILES))

# IPv6 files
SRC_C += $(notdir $(CORE6FILES))

# Network interface files
SRC_C += $(notdir $(NETIFFILES))

INC_DIR += $(LWIP_PORT_DIR)/include/lwip \
           $(LWIPDIR)/include \
           $(LWIPDIR)/include/ipv4 \
           $(LWIPDIR)/include/api \
           $(LWIPDIR)/include/netif \
           $(REP_DIR)/src/lib/lwip/include


SRC_C += nic_netif.c

SRC_CC = rand.cc \
         printf.cc \
         socket.cc \
         socket_tcp.cc \
         socket_udp.cc \
         sys_arch.cc \

SRC_CC += genode_c_api/nic_client.cc

vpath %.cc $(REP_DIR)/src/lib/lwip
vpath %.c  $(REP_DIR)/src/lib/lwip
vpath %.c  $(sort $(dir \
	$(COREFILES) $(CORE4FILES) $(CORE6FILES) $(NETIFFILES)))

C_API = $(dir $(call select_from_repositories,src/lib/genode_c_api))

vpath genode_c_api/nic_client.cc $(C_API)

#XXX: Remove
CC_CXX_WARN_STRICT =
