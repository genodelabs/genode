WIFI_DRV_DIR := $(call select_from_ports,linux)/src/lib/wifi
ifeq ($(wildcard $(WIFI_DRV_DIR)),)
WIFI_DRV_DIR := $(call select_from_repositories,src/lib/wifi)
endif

INC_DIR += $(WIFI_DRV_DIR)

LD_OPT += --version-script=$(WIFI_DRV_DIR)/symbol.map

vpath %.c $(WIFI_DRV_DIR)
vpath %.cc $(WIFI_DRV_DIR)

$(LIB).lib.so: $(WIFI_DRV_DIR)/symbol.map

#
# Generic driver code shared between all instances
#

SHARED_LIB := yes

LIBS += base jitterentropy

SRC_CC  += firmware.cc
SRC_CC  += socket_call.cc
SRC_CC  += wlan.cc

SRC_C   += dummies.c
SRC_C   += lx_emul.c
SRC_C   += lx_user.c
SRC_C   += uplink.c

CC_OPT_dummies           += -DKBUILD_MODNAME='"dummies"'
CC_OPT_generated_dummies += -DKBUILD_MODNAME='"generated_dummies"'

CC_OPT_lx_socket_call += -DKBUILD_MODNAME='"lx_socket_call"'
SRC_C   += lx_socket_call.c

#
# Generic Linux options
#

CC_C_OPT += -Wno-address-of-packed-member

# need net/rfkill/rfkill.h
CC_OPT_lx_emul += -I$(LX_SRC_DIR)

CC_C_OPT += -DCONFIG_RFKILL_INPUT

#
# Genode C-API backends
#

SRC_CC  += genode_c_api/uplink.cc
vpath genode_c_api/uplink.cc $(subst /genode_c_api,,$(call select_from_repositories,src/lib/genode_c_api))

SRC_CC += genode_c_api/mac_address_reporter.cc
vpath genode_c_api/mac_address_reporter.cc $(dir $(call select_from_repositories,src/lib/genode_c_api))
