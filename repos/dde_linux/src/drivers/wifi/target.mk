TARGET   = wifi_drv
SRC_CC   = main.cc
LIBS     = wifi wpa_supplicant server

# needed for firmware.h
INC_DIR += $(REP_DIR)/src/lib/wifi/include

#
# Copy all firmware images to the build directory
#
WIFI_CONTRIB_DIR := $(call select_from_ports,dde_linux)
$(TARGET): firmware_images
firmware_images:
	$(VERBOSE)for img in $(WIFI_CONTRIB_DIR)/firmware/*.ucode; do \
		cp $$img $(PWD)/bin; \
	done
