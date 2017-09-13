content: include lib/symbols/libiconv LICENSE

PORT_DIR := $(call port_dir,$(REP_DIR)/ports/libiconv)

include:
	mkdir $@
	cp -r $(PORT_DIR)/include/iconv/* $@/

lib/symbols/libiconv:
	$(mirror_from_rep_dir)

LICENSE:
	cp $(PORT_DIR)/src/lib/libiconv/COPYING.LIB $@

