content: src/lib/libssh lib/mk LICENSE

PORT_DIR := $(call port_dir,$(REP_DIR)/ports/libssh)

src/lib/libssh:
	mkdir -p $@
	cp -r $(PORT_DIR)/src/lib/libssh/* $@
	cp -r $(REP_DIR)/src/lib/libssh/config.h $@

lib/mk:
	mkdir -p $@
	cp $(REP_DIR)/lib/mk/libssh.mk $@

LICENSE:
	cp $(PORT_DIR)/src/lib/libssh/COPYING $@
