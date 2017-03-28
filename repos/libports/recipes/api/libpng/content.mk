content: include lib/symbols/libpng LICENSE

PORT_DIR := $(call port_dir,$(REP_DIR)/ports/libpng)

include:
	mkdir $@
	cp -r $(PORT_DIR)/include/libpng/* $@/

lib/symbols/libpng:
	$(mirror_from_rep_dir)

LICENSE:
	cp $(PORT_DIR)/src/lib/libpng/LICENSE $@

