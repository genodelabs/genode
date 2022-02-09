REQUIRES = x86

TARGET   = wifi_drv
SRC_CC   = main.cc wpa.cc
LIBS     = base wifi iwl_firmware
LIBS    += wpa_supplicant libc nic_driver
LIBS    += libcrypto libssl wpa_driver_nl80211

# needed for firmware.h
INC_DIR += $(REP_DIR)/src/lib/wifi/include
INC_DIR += $(PRG_DIR)

CC_CXX_WARN_STRICT =
