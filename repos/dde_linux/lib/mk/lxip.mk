SHARED_LIB = yes

LIB_DIR     = $(REP_DIR)/src/lib/lxip
LIB_INC_DIR = $(LIB_DIR)/include

LIBS += lx

LX_CONTRIB_DIR := $(call select_from_ports,dde_linux)/src/lib/lxip
NET_DIR        := $(LX_CONTRIB_DIR)/net

#
# The order of include-search directories is important, we need to look into
# 'contrib' before falling back to our custom 'lx_emul.h' header.
#
INC_DIR += $(LIB_INC_DIR)
INC_DIR += $(LX_CONTRIB_DIR)/include $(LX_CONTRIB_DIR)/include/uapi \
           $(LX_CONTRIB_DIR)

CC_OLEVEL = -O2

SETUP_SUFFIX =
CC_OPT += -DSETUP_SUFFIX=$(SETUP_SUFFIX)

CC_OPT += -U__linux__ -D__KERNEL__
CC_OPT += -DCONFIG_INET -DCONFIG_BASE_SMALL=0 -DCONFIG_DEBUG_LOCK_ALLOC \
          -DCONFIG_IP_PNP_DHCP

CC_WARN = -Wall -Wno-unused-variable -Wno-uninitialized \
          -Wno-unused-function -Wno-overflow -Wno-pointer-arith \
          -Wno-sign-compare

CC_C_OPT  += -Wno-unused-but-set-variable -Wno-pointer-sign

CC_C_OPT  += -include $(LIB_INC_DIR)/lx_emul.h
CC_CXX_OPT = -fpermissive

SRC_CC = dummies.cc env.cc lxcc_emul.cc nic_handler.cc socket_handler.cc \
         timer_handler.cc

SRC_C += driver.c dummies_c.c init.c lxc_emul.c socket.c

SRC_C += net/802/p8023.c
SRC_C += $(addprefix net/core/,$(notdir $(wildcard $(NET_DIR)/core/*.c)))
SRC_C += $(addprefix net/ipv4/,$(notdir $(wildcard $(NET_DIR)/ipv4/*.c)))
SRC_C += net/ethernet/eth.c
SRC_C += net/netlink/af_netlink.c
SRC_C += net/sched/sch_generic.c
SRC_C += lib/checksum.c
SRC_C += drivers/net/loopback.c

# DHCP support
SRC_C += net/ipv4/ipconfig.c

#SRC_C = net/ipv4/inet_connection_sock.c

#
# Determine the header files included by the contrib code. For each
# of these header files we create a symlink to 'lx_emul.h'.
#
GEN_INCLUDES := $(shell grep -rh "^\#include .*\/" $(LX_CONTRIB_DIR) |\
                        sed "s/^\#include [^<\"]*[<\"]\([^>\"]*\)[>\"].*/\1/" | sort | uniq)

#
# Filter out original Linux headers that exist in the contrib directory
#
NO_GEN_INCLUDES := $(shell cd $(LX_CONTRIB_DIR); find -name "*.h" | sed "s/.\///" | sed "s/.*include\///")
GEN_INCLUDES    := $(filter-out $(NO_GEN_INCLUDES),$(GEN_INCLUDES))

#
# Put Linux headers in 'GEN_INC' dir, since some include use "../../" paths use
# three level include hierarchy
#
GEN_INC         := $(shell pwd)/include/include/include

$(shell mkdir -p $(GEN_INC))

GEN_INCLUDES    := $(addprefix $(GEN_INC)/,$(GEN_INCLUDES))
INC_DIR         += $(GEN_INC)

#
# Make sure to create the header symlinks prior building
#
$(SRC_C:.c=.o) $(SRC_CC:.cc=.o): $(GEN_INCLUDES)

net/ethernet/eth.o: SETUP_SUFFIX="_eth"

$(GEN_INCLUDES):
	$(VERBOSE)mkdir -p $(dir $@)
	$(VERBOSE)ln -s $(LIB_INC_DIR)/lx_emul.h $@

vpath %.c $(LX_CONTRIB_DIR)
vpath %.c $(LIB_DIR)
vpath %.cc $(LIB_DIR)
