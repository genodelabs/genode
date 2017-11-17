content: include lib/symbols/jbig2dec LICENSE

PORT_DIR := $(call port_dir,$(REP_DIR)/ports/jbig2dec)

include:
	mkdir $@
	cp -r $(PORT_DIR)/include/jbig2dec/* $@/

lib/symbols/jbig2dec:
	$(mirror_from_rep_dir)

LICENSE:
	cp $(PORT_DIR)/src/lib/jbig2dec/LICENSE $@
