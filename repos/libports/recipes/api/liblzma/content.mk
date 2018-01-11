content: include lib/symbols/liblzma LICENSE

PORT_DIR := $(call port_dir,$(REP_DIR)/ports/xz)

lib/symbols/liblzma:
	$(mirror_from_rep_dir)

include:
	cp -r $(PORT_DIR)/include/liblzma $@

LICENSE:
	echo "liblzma is in the public domain" > $@

