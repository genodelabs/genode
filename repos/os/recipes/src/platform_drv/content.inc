include $(GENODE_DIR)/repos/base/recipes/src/content.inc

GENERIC_SRC_DIR := $(GENODE_DIR)/repos/os/src/drivers/platform
GENERIC_INC_DIR := $(GENODE_DIR)/repos/os/include/pci

GENERIC_SRC_FILES := $(filter-out target.mk,$(filter-out main.cc,$(notdir $(wildcard $(GENERIC_SRC_DIR)/*.*))))
GENERIC_HDR_FILES := $(notdir $(wildcard $(GENERIC_INC_DIR)/*.h))

MIRROR_FROM_OS_DIR := $(addprefix src/drivers/platform/,$(GENERIC_SRC_FILES)) \
                      $(addprefix include/pci/,$(GENERIC_HDR_FILES))

content: $(MIRROR_FROM_OS_DIR)

$(MIRROR_FROM_OS_DIR):
	mkdir -p $(dir $@)
	cp -r $(GENODE_DIR)/repos/os/$@ $@

