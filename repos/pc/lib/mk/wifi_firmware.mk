#
# Pseudo library to copy wireless LAN firmware to build directory
#

FW_CONTRIB_DIR := $(call select_from_ports,linux-firmware)

IMAGES  := $(notdir $(wildcard $(FW_CONTRIB_DIR)/firmware/*.ucode))
IMAGES  += $(notdir $(wildcard $(FW_CONTRIB_DIR)/firmware/*.pnvm))
IMAGES  += $(notdir $(wildcard $(FW_CONTRIB_DIR)/firmware/*.bin))
IMAGES  += $(notdir $(wildcard $(FW_CONTRIB_DIR)/firmware/*.db))
IMAGES  += $(notdir $(wildcard $(FW_CONTRIB_DIR)/firmware/*.p7s))
BIN_DIR := $(BUILD_BASE_DIR)/bin
FW_DIR  := $(FW_CONTRIB_DIR)/firmware

CUSTOM_TARGET_DEPS += $(addprefix $(BIN_DIR)/,$(IMAGES))

$(BIN_DIR)/%.bin: $(FW_DIR)/%.bin
	$(VERBOSE)cp $^ $@

$(BIN_DIR)/%.ucode: $(FW_DIR)/%.ucode
	$(VERBOSE)cp $^ $@

$(BIN_DIR)/%.pnvm: $(FW_DIR)/%.pnvm
	$(VERBOSE)cp $^ $@

$(BIN_DIR)/%.db: $(FW_DIR)/%.db
	$(VERBOSE)cp $^ $@

$(BIN_DIR)/%.p7s: $(FW_DIR)/%.p7s
	$(VERBOSE)cp $^ $@

CC_CXX_WARN_STRICT =
