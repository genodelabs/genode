content: src/lib/libpng lib/mk/libpng.mk LICENSE

PORT_DIR := $(call port_dir,$(REP_DIR)/ports/libpng)

src/lib/libpng:
	$(mirror_from_rep_dir)
	cp -r $(PORT_DIR)/src/lib/libpng/* $@
	echo "LIBS = libpng" > $@/target.mk

lib/mk/libpng.mk:
	$(mirror_from_rep_dir)

LICENSE:
	cp $(PORT_DIR)/src/lib/libpng/LICENSE $@

