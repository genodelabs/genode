REQUIRES := x86

TARGET_LIB_DIR := $(REP_DIR)/src/lib/wifi

SHARED_LIB := yes

LD_OPT += --version-script=$(TARGET_LIB_DIR)/symbol.map

LIBS    += base jitterentropy pc_linux_generated pc_lx_emul
INC_DIR := $(TARGET_LIB_DIR)
SRC_CC  += wlan.cc
SRC_CC  += misc.cc
SRC_CC  += firmware.cc
SRC_CC  += socket_call.cc
SRC_CC  += lx_emul/random.cc
SRC_CC  += time.cc

SRC_C   += dummies.c
SRC_C   += lx_emul.c
SRC_C   += lx_user.c
SRC_C   += uplink.c

CC_OPT_lx_socket_call += -DKBUILD_MODNAME='"lx_socket_call"'
SRC_C   += lx_socket_call.c

SRC_C   += $(notdir $(wildcard $(TARGET_LIB_DIR)/generated_dummies.c))
SRC_C   += lx_emul/common_dummies.c
SRC_C   += lx_emul/spec/x86/pci.c

CC_C_OPT += -I$(LX_SRC_DIR)/drivers/net/wireless/intel/iwlwifi
CC_C_OPT += -I$(LX_SRC_DIR)/include/linux

CC_C_OPT += -Wno-address-of-packed-member

# need net/rfkill/rfkill.h
CC_OPT_lx_emul += -I$(LX_SRC_DIR)

CC_C_OPT += -DCONFIG_RFKILL_INPUT

#CC_OPT += -DCONFIG_IWLWIFI_DEBUG


SRC_C   += lx_emul/shadow/lib/kobject_uevent.c
vpath %.c  $(REP_DIR)/src/lib/pc
vpath %.cc $(REP_DIR)/src/lib/pc

vpath %.c $(TARGET_LIB_DIR)
vpath %.cc $(TARGET_LIB_DIR)

$(LIB).lib.so: $(TARGET_LIB_DIR)/symbol.map

#
# Genode C-API backends
#

SRC_CC  += genode_c_api/uplink.cc

vpath genode_c_api/uplink.cc $(subst /genode_c_api,,$(call select_from_repositories,src/lib/genode_c_api))
