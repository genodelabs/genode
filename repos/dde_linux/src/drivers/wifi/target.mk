TARGET  := wifi
SRC_CC  := main.cc wpa.cc access_firmware.cc
LIBS    := base wifi
LIBS    += libc
LIBS    += wpa_supplicant
LIBS    += libcrypto libssl wpa_driver_nl80211

INC_DIR += $(PRG_DIR)

CC_CXX_WARN_STRICT :=
