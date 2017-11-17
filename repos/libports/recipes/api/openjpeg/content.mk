content: include lib/symbols/openjpeg LICENSE

PORT_DIR := $(call port_dir,$(REP_DIR)/ports/openjpeg)

include:
	mkdir $@
	cp -r $(PORT_DIR)/include/openjpeg/* $@/

lib/symbols/openjpeg:
	$(mirror_from_rep_dir)

LICENSE:
	cp $(PORT_DIR)/src/lib/openjpeg/LICENSE $@
