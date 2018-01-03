REQUIRES = x86

TARGET   = wifi_drv
SRC_CC   = main.cc
LIBS     = base wifi iwl_firmware wpa_supplicant

# needed for firmware.h
INC_DIR += $(REP_DIR)/src/lib/wifi/include

CC_CXX_WARN_STRICT =
