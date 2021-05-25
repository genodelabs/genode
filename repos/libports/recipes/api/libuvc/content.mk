content: include lib/symbols/libuvc LICENSE

PORT_DIR := $(call port_dir,$(REP_DIR)/ports/libuvc)

include:
	mkdir $@
	cp -r $(PORT_DIR)/include/* $@/

lib/symbols/libuvc:
	$(mirror_from_rep_dir)

LICENSE:
	cp $(PORT_DIR)/src/lib/libuvc/LICENSE.txt $@

