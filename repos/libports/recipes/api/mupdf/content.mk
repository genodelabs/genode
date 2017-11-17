content: include lib/symbols/mupdf LICENSE

PORT_DIR := $(call port_dir,$(REP_DIR)/ports/mupdf)

include:
	mkdir $@
	cp -r $(PORT_DIR)/include/mupdf/* $@/

lib/symbols/mupdf:
	$(mirror_from_rep_dir)

LICENSE:
	cp $(PORT_DIR)/src/lib/mupdf/COPYING $@
