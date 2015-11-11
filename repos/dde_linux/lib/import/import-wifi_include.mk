WIFI_CONTRIB_DIR := $(call select_from_ports,dde_linux)/src/lib/wifi
DRIVERS_DIR      := $(WIFI_CONTRIB_DIR)/drivers
WIFI_DIR         := $(WIFI_CONTRIB_DIR)/net

LIB_DIR          := $(REP_DIR)/src/lib/wifi
LIB_INC_DIR      := $(LIB_DIR)/include

#
# The order of include-search directories is important, we need to look into
# 'contrib' before falling back to our custom 'lx_emul.h' header.
#
INC_DIR += $(LIB_INC_DIR)
INC_DIR += $(WIFI_CONTRIB_DIR)/include $(WIFI_CONTRIB_DIR)/include/uapi \
INC_DIR += $(LIB_DIR)

INC_DIR += $(WIFI_CONTRIB_DIR)/$(DRV_DIR_IWLWIFI)
INC_DIR += $(WIFI_CONTRIB_DIR)/$(DRV_DIR_IWLEGACY)

INC_DIR += $(LIB_CACHE_DIR)/wifi_include/include/include/include
