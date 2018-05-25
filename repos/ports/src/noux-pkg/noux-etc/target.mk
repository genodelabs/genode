#
# This is no actual noux-pkg but a collection of files
# which are essential for noux/net.
#

BUILD_BIN_DIR = ../../bin
TARGET_DIR    = $(BUILD_BIN_DIR)/noux-etc/etc

ETC_CONTRIB_DIR := $(call select_from_ports,etc)/src/noux-pkg/etc

ETC_FILES = hosts \
            nsswitch.conf \
            protocols \
            services

TARGET = noux-etc.build

copy-contrib-files:
	$(VERBOSE)mkdir -p $(TARGET_DIR)
	$(VERBOSE)for i in $(ETC_FILES); do \
		cp $(ETC_CONTRIB_DIR)/$$i $(TARGET_DIR) ; \
	done

generate-files: copy-contrib-files

$(BUILD_BIN_DIR)/$(TARGET): generate-files

$(TARGET): $(BUILD_BIN_DIR)/$(TARGET)
