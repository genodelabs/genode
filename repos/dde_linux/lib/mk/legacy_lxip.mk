SHARED_LIB = yes

LIB_DIR     = $(REP_DIR)/src/lib/legacy_lxip
LIB_INC_DIR = $(LIB_DIR)/include

LIBS += lxip_include format

LX_CONTRIB_DIR := $(call select_from_ports,dde_linux)/src/lib/lxip
NET_DIR        := $(LX_CONTRIB_DIR)/net

SETUP_SUFFIX =
CC_OPT += -DSETUP_SUFFIX=$(SETUP_SUFFIX)

CC_OPT += -U__linux__ -D__KERNEL__
CC_OPT += -DCONFIG_INET -DCONFIG_BASE_SMALL=0 -DCONFIG_DEBUG_LOCK_ALLOC \
          -DCONFIG_IP_PNP_DHCP

CC_WARN = -Wall -Wno-unused-variable -Wno-uninitialized \
          -Wno-unused-function -Wno-overflow -Wno-pointer-arith \
          -Wno-sign-compare -Wno-builtin-declaration-mismatch

CC_C_OPT  += -std=gnu89
CC_C_OPT  += -Wno-unused-but-set-variable -Wno-pointer-sign

CC_C_OPT  += -include $(LIB_INC_DIR)/lx_emul.h
CC_CXX_OPT += -fpermissive

SRC_CC = dummies.cc lxcc_emul.cc nic_handler.cc \
         timer_handler.cc random.cc

SRC_CC += malloc.cc printf.cc bug.cc env.cc

SRC_C += driver.c dummies_c.c lxc_emul.c

SRC_C += net/802/p8023.c
SRC_C += $(addprefix net/core/,$(notdir $(wildcard $(NET_DIR)/core/*.c)))
SRC_C += $(addprefix net/ipv4/,$(notdir $(wildcard $(NET_DIR)/ipv4/*.c)))
SRC_C += net/ethernet/eth.c
SRC_C += net/netlink/af_netlink.c
SRC_C += net/sched/sch_generic.c
SRC_C += lib/checksum.c
SRC_C += lib/rhashtable.c
SRC_C += drivers/net/loopback.c

#SRC_C = net/ipv4/inet_connection_sock.c

net/ethernet/eth.o: SETUP_SUFFIX="_eth"

vpath %.c $(LX_CONTRIB_DIR)
vpath %.c $(LIB_DIR)
vpath %.cc $(LIB_DIR)
vpath %.cc $(REP_DIR)/src/lib/legacy/lx_kit

CC_CXX_WARN_STRICT =
