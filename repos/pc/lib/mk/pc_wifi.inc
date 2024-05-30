REQUIRES := x86

include $(call select_from_repositories,lib/mk/wifi.inc)

TARGET_LIB_DIR := $(REP_DIR)/src/lib/pc_wifi

LIBS    += pc_linux_generated pc_lx_emul

INC_DIR += $(TARGET_LIB_DIR)

SRC_CC  += dtb_helper_no_dtb.cc
SRC_C   += lx_emul_pci.c

SRC_CC  += lx_emul/random.cc

SRC_C   += $(notdir $(wildcard $(TARGET_LIB_DIR)/generated_dummies.c))
SRC_C   += lx_emul/common_dummies.c
SRC_C   += lx_emul/shadow/lib/kobject_uevent.c

CC_C_OPT += -I$(LX_SRC_DIR)/drivers/net/wireless/intel/iwlwifi
CC_C_OPT += -I$(LX_SRC_DIR)/drivers/net/wireless/realtek/rtlwifi
CC_C_OPT += -I$(LX_SRC_DIR)/drivers/net/wireless/realtek/rtlwifi/rtl8188ee
CC_C_OPT += -I$(LX_SRC_DIR)/drivers/net/wireless/ath/
CC_C_OPT += -I$(LX_SRC_DIR)/drivers/net/wireless/ath/ath9k
CC_C_OPT += -I$(LX_SRC_DIR)/include/linux

KBUILD_MODNAME_drivers/net/wireless/realtek/rtlwifi/rtl8188ce/sw := rtl8188ce
KBUILD_MODNAME_drivers/net/wireless/realtek/rtlwifi/rtl8188ee/sw := rtl8188ee

vpath %.c  $(REP_DIR)/src/lib/pc
vpath %.cc $(REP_DIR)/src/lib/pc

vpath %.c $(TARGET_LIB_DIR)
vpath %.cc $(TARGET_LIB_DIR)
