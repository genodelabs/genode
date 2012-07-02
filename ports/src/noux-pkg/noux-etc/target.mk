#
# This is no actual noux-pkg but a collection of files
# which are essential for noux/net.
#

BUILD_BIN_DIR = ../../bin
TARGET_DIR    = $(BUILD_BIN_DIR)/noux-etc/etc

ETC_CONTRIB_DIR = $(REP_DIR)/contrib/etc-8.2.0

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

generate-files:
	$(VERBOSE)echo 'nameserver 8.8.8.8' > $(TARGET_DIR)/resolv.conf

$(BUILD_BIN_DIR)/$(TARGET): copy-contrib-files generate-files

$(TARGET): $(BUILD_BIN_DIR)/$(TARGET)
