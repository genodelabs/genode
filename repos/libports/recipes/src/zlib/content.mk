content: src/lib/zlib lib/mk/zlib.mk LICENSE

PORT_DIR := $(call port_dir,$(REP_DIR)/ports/zlib)

src/lib/zlib:
	mkdir -p $@
	cp -r $(PORT_DIR)/src/lib/zlib/* $@
	echo "LIBS = zlib" > $@/target.mk

lib/mk/zlib.mk:
	$(mirror_from_rep_dir)

LICENSE:
	echo "zlib license, see src/lib/zlib/README" > $@

