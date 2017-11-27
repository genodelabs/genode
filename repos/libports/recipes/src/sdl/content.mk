content: src/lib/sdl/target.mk lib/mk LICENSE

PORT_DIR := $(call port_dir,$(REP_DIR)/ports/sdl)

src/lib/sdl:
	mkdir -p $@
	cp -r $(PORT_DIR)/src/lib/sdl/* $@
	cp -r $(REP_DIR)/src/lib/sdl/* $@

src/lib/sdl/target.mk: src/lib/sdl
	echo "LIBS += sdl" > $@

lib/mk:
	mkdir -p $@
	cp $(REP_DIR)/$@/sdl.mk $@

LICENSE:
	cp $(PORT_DIR)/src/lib/sdl/COPYING $@
