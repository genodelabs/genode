REQUIRES = x86

TARGET   = wifi_drv
SRC_CC   = main.cc
LIBS     = wifi iwl_firmware wpa_supplicant

# needed for firmware.h
INC_DIR += $(REP_DIR)/src/lib/wifi/include
