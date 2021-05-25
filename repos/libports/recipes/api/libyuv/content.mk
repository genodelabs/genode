content: include lib/symbols/libyuv LICENSE

PORT_DIR := $(call port_dir,$(REP_DIR)/ports/libyuv)

include:
	mkdir $@
	cp -r $(PORT_DIR)/include/* $@/

lib/symbols/libyuv:
	$(mirror_from_rep_dir)

LICENSE:
	cp $(PORT_DIR)/src/lib/libyuv/LICENSE $@

