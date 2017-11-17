content: include lib/symbols/freetype LICENSE

PORT_DIR := $(call port_dir,$(REP_DIR)/ports/freetype)

include:
	mkdir $@
	cp -r $(PORT_DIR)/include/* $@

lib/symbols/freetype:
	$(mirror_from_rep_dir)

LICENSE:
	cp $(PORT_DIR)/src/lib/freetype/contrib/docs/LICENSE.TXT $@

