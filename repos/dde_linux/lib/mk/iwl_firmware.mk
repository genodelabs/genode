#
# Pseudo library to copy Intel Wireless firmware to build directory
#

WIFI_CONTRIB_DIR := $(call select_from_ports,dde_linux)

IMAGES  := $(notdir $(wildcard $(WIFI_CONTRIB_DIR)/firmware/*.ucode))
IMAGES  += $(notdir $(wildcard $(WIFI_CONTRIB_DIR)/firmware/*.db))
BIN_DIR := $(BUILD_BASE_DIR)/bin
FW_DIR  := $(WIFI_CONTRIB_DIR)/firmware

all: $(addprefix $(BIN_DIR)/,$(IMAGES))

$(BIN_DIR)/%.ucode: $(FW_DIR)/%.ucode
	$(VERBOSE)cp $^ $@

$(BIN_DIR)/%.db: $(FW_DIR)/%.db
	$(VERBOSE)cp $^ $@


CC_CXX_WARN_STRICT =
