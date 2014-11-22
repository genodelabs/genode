TARGET   = wifi_drv

SRC_CC   = main.cc

# needed for firmware.h
INC_DIR += $(REP_DIR)/src/lib/wifi/include

LIBS     = wifi wpa_supplicant server
