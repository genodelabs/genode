LIB_DIR          := $(REP_DIR)/src/lib/wifi
LIB_INC_DIR      := $(LIB_DIR)/include

WIFI_CONTRIB_DIR := $(call select_from_ports,dde_linux)/src/lib/wifi
DRIVERS_DIR      := $(WIFI_CONTRIB_DIR)/drivers
WIFI_DIR         := $(WIFI_CONTRIB_DIR)/net

SHARED_LIB = yes

#
# wifi_include *must* be the first library, otherwise the include
# order is wrong
#
LIBS  += wifi_include lx_kit_setjmp libc

LD_OPT += --version-script=$(LIB_DIR)/symbol.map

SRC_CC += dummies.cc init.cc lxcc_emul.cc nic.cc socket_call.cc

# lx_kit
SRC_CC += mapped_io_mem_range.cc irq.cc pci.cc malloc.cc scheduler.cc \
          work.cc timer.cc printf.cc env.cc

SRC_C += lxc_emul.c

CC_WARN  =
CC_OPT  += -fno-builtin-toupper

CC_C_OPT   += -std=gnu89
CC_C_OPT   += -include $(LIB_INC_DIR)/lx_emul.h
CC_C_OPT   += -Wall -Wno-unused-variable -Wno-unused-function -Wno-uninitialized
CC_C_OPT   += -Wno-unused-but-set-variable -Wno-pointer-sign -Wno-cast-qual
CC_CXX_OPT += -Wall -fpermissive

CC_OPT += -U__linux__ -D __KERNEL__ -DCONFIG_PCI
#CC_OPT += -DCONFIG_INET
CC_OPT += -DCONFIG_NET -DCONFIG_NET_NS -DCONFIG_WLAN #-D__MAC80211_DRIVER_TRACE

# needed for firmware loading
CC_OPT += -DCONFIG_FW_LOADER

# bling bling
CC_OPT += -DCONFIG_LEDS_TRIGGERS -DCONFIG_MAC80211_LEDS

# mesh
CC_OPT += -DCONFIG_MAC80211_MESH

# power management
CC_OPT += -DCONFIG_PM -DCONFIG_PM_SLEEP

# rfkill
CC_OPT += -DCONFIG_RFKILL
# choose default pid algorithm
CC_OPT += -DCONFIG_MAC80211_RC_PID -DCONFIG_MAC80211_RC_DEFAULT=\"pid\"

#
# Suffix of global 'module_init' function
#
MOD_SUFFIX =
CC_OPT += -DMOD_SUFFIX=$(MOD_SUFFIX)

SRC_C += lib/ctype.c
SRC_C += lib/crc32.c

# crypto
SRC_C += $(addprefix crypto/, $(notdir $(wildcard $(WIFI_CONTRIB_DIR)/crypto/*.c)))

# net
SRC_C += net/core/datagram.c
SRC_C += net/core/rtnetlink.c
SRC_C += net/core/skbuff.c
SRC_C += net/core/sock.c
SRC_C += net/ethernet/eth.c
SRC_C += net/netlink/af_netlink.c
SRC_C += net/netlink/genetlink.c
SRC_C += net/packet/af_packet.c

# netlink
SRC_C += lib/nlattr.c
SRC_C += lib/rbtree.c
SRC_C += lib/rhashtable.c

# wifi mac80211
SRC_C += $(addprefix net/mac80211/, $(notdir $(wildcard $(WIFI_DIR)/mac80211/*.c)))
CC_OPT += -DCONFIG_MAC80211_RC_MINSTREL -DCONFIG_MAC80211_RC_MINSTREL_HT
CC_OPT += -DCONFIG_MAC80211_STA_HASH_MAX_SIZE=0

# wifi rfkill
SRC_C += $(addprefix net/rfkill/, $(notdir $(wildcard $(WIFI_DIR)/rfkill/*.c)))
INC_DIR += $(WIFI_DIR)/rfkill

# wifi cfg80211
SRC_C   += $(addprefix net/wireless/, $(notdir $(wildcard $(WIFI_DIR)/wireless/*.c)))
INC_DIR += $(WIFI_DIR)/wireless
CC_OPT  += -DCONFIG_CFG80211_REG_RELAX_NO_IR=0

# led driver
SRC_C += $(addprefix drivers/leds/, $(notdir $(wildcard $(DRIVERS_DIR)/leds/*.c)))
CC_OPT += -DCONFIG_LEDS_TRIGGERS

CC_OPT += -D__CHECK_ENDIAN__

# iwlwifi driver
DRV_DIR_IWLWIFI := drivers/net/wireless/iwlwifi
SRC_C += $(addprefix $(DRV_DIR_IWLWIFI)/,      $(notdir $(wildcard $(WIFI_CONTRIB_DIR)/$(DRV_DIR_IWLWIFI)/*.c)))
SRC_C += $(addprefix $(DRV_DIR_IWLWIFI)/dvm/,  $(notdir $(wildcard $(WIFI_CONTRIB_DIR)/$(DRV_DIR_IWLWIFI)/dvm/*.c)))
SRC_C += $(addprefix $(DRV_DIR_IWLWIFI)/mvm/,  $(notdir $(wildcard $(WIFI_CONTRIB_DIR)/$(DRV_DIR_IWLWIFI)/mvm/*.c)))
SRC_C += $(addprefix $(DRV_DIR_IWLWIFI)/pcie/, $(notdir $(wildcard $(WIFI_CONTRIB_DIR)/$(DRV_DIR_IWLWIFI)/pcie/*.c)))
CC_OPT += -DCONFIG_IWLMVM -DCONFIG_IWLDVM -DCONFIG_IWLWIFI_LEDS
CC_OPT += -DCONFIG_IWLWIFI_DEBUG

vpath %.c  $(DRIVERS_DIR)
vpath %.c  $(WIFI_CONTRIB_DIR)/drivers/net/wireless/
vpath %.c  $(WIFI_DIR)
vpath %.c  $(WIFI_CONTRIB_DIR)
vpath %.c  $(LIB_DIR)
vpath %.cc $(LIB_DIR)
vpath %.cc $(REP_DIR)/src/lx_kit

# vi: set ft=make :
