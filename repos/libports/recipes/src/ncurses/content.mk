content: src/lib/ncurses lib/mk/ncurses.mk LICENSE

PORT_DIR := $(call port_dir,$(REP_DIR)/ports/ncurses)

src/lib/ncurses:
	mkdir -p $@
	cp -r $(PORT_DIR)/src/lib/ncurses/* $@
	echo "LIBS = ncurses" > $@/target.mk

lib/mk/ncurses.mk:
	$(mirror_from_rep_dir)

LICENSE:
	cp $(PORT_DIR)/src/lib/ncurses/README $@

