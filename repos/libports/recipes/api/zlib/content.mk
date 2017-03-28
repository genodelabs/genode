content: include lib/symbols/zlib LICENSE

PORT_DIR := $(call port_dir,$(REP_DIR)/ports/zlib)

include:
	mkdir $@
	cp -r $(PORT_DIR)/include/zlib/* $@/

lib/symbols/zlib:
	$(mirror_from_rep_dir)

LICENSE:
	echo "zlib license" > $@

