SRC_DIR = src/drivers/acpi
include $(GENODE_DIR)/repos/base/recipes/src/content.inc

content: $(GENODE_DIR)/repos/os/include/smbios
	mkdir -p include
	cp -r $< include/smbios
