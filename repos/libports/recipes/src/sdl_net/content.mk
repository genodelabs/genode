content: src/lib/sdl_net/target.mk lib/mk LICENSE

PORT_DIR := $(call port_dir,$(REP_DIR)/ports/sdl_net)

src/lib/sdl_net:
	mkdir -p $@
	cp -r $(PORT_DIR)/src/lib/sdl_net/* $@

src/lib/sdl_net/target.mk: src/lib/sdl_net
	echo "LIBS += sdl_net" > $@

lib/mk:
	mkdir -p $@
	cp $(REP_DIR)/$@/sdl_net.mk $@

LICENSE:
	cp $(PORT_DIR)/src/lib/sdl_net/COPYING $@
