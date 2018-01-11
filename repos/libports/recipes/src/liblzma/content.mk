content: src/xz src/lib/liblzma lib/mk/liblzma.mk LICENSE

PORT_DIR := $(call port_dir,$(REP_DIR)/ports/xz)

include:
	$(PORT_DIR)/include/liblzma $@

src/lib/liblzma:
	mkdir -p $@
	cp $(REP_DIR)/src/lib/liblzma/config.h $@
	echo "LIBS = liblzma" > $@/target.mk

src/xz:
	mkdir -p $@
	cp -r $(PORT_DIR)/src/xz/* $@

lib/mk/liblzma.mk:
	$(mirror_from_rep_dir)

LICENSE:
	cp $(PORT_DIR)/src/xz/COPYING $@

