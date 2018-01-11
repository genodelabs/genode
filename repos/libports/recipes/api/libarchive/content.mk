content: include lib/symbols/libarchive LICENSE

PORT_DIR := $(call port_dir,$(REP_DIR)/ports/libarchive)

lib/symbols/libarchive:
	$(mirror_from_rep_dir)

include:
	cp -r $(PORT_DIR)/include/libarchive $@

LICENSE:
	cp $(PORT_DIR)/src/lib/libarchive/COPYING $@

