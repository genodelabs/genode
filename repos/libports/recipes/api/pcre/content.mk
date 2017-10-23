content: include lib/symbols/pcre LICENCE

PORT_DIR := $(call port_dir,$(REP_DIR)/ports/pcre)

include:
	mkdir $@
	cp -r $(PORT_DIR)/include/pcre/* $@/

lib/symbols/pcre:
	$(mirror_from_rep_dir)

LICENCE:
	cp $(PORT_DIR)/src/lib/pcre/LICENCE $@

