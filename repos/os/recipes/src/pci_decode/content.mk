SRC_DIR = src/app/pci_decode
include $(GENODE_DIR)/repos/base/recipes/src/content.inc

content: $(GENODE_DIR)/repos/os/include/pci
	mkdir -p include
	cp -r $< include/
