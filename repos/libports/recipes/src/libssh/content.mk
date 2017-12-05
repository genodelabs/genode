content: src/lib/libssh/target.mk lib/mk LICENSE

PORT_DIR := $(call port_dir,$(REP_DIR)/ports/libssh)

src/lib/libssh:
	mkdir -p $@
	cp -r $(PORT_DIR)/src/lib/libssh/* $@
	cp -r $(REP_DIR)/src/lib/libssh/config.h $@

src/lib/libssh/target.mk: src/lib/libssh
	echo "LIBS += libssh" > $@

lib/mk:
	mkdir -p $@
	cp $(REP_DIR)/lib/mk/libssh.mk $@

LICENSE:
	cp $(PORT_DIR)/src/lib/libssh/COPYING $@
