content: include lib/symbols/ncurses LICENSE

PORT_DIR := $(call port_dir,$(REP_DIR)/ports/ncurses)

include:
	mkdir $@
	cp -r $(PORT_DIR)/include/ncurses/* $@/
	cp -r $(REP_DIR)/include/ncurses/* $@/

lib/symbols/ncurses:
	$(mirror_from_rep_dir)

LICENSE:
	cp $(PORT_DIR)/src/lib/ncurses/README $@

