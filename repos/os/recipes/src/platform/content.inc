include $(GENODE_DIR)/repos/base/recipes/src/content.inc

GENERIC_SRC_DIR := $(GENODE_DIR)/repos/os/src/driver/platform

GENERIC_SRC_FILES := $(filter-out target.mk,$(filter-out main.cc,$(notdir $(wildcard $(GENERIC_SRC_DIR)/*.*))))

MIRROR_FROM_OS_DIR := $(addprefix src/driver/platform/,$(GENERIC_SRC_FILES))

content: $(MIRROR_FROM_OS_DIR)

$(MIRROR_FROM_OS_DIR):
	mkdir -p $(dir $@)
	cp -r $(GENODE_DIR)/repos/os/$@ $@

