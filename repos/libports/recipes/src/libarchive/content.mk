content: src/lib/libarchive lib/mk/libarchive.mk LICENSE

PORT_DIR := $(call port_dir,$(REP_DIR)/ports/libarchive)

include:
	$(PORT_DIR)/include/libarchive $@

src/lib/libarchive:
	mkdir -p $@
	cp -r $(PORT_DIR)/src/lib/libarchive/* $@
	cp $(REP_DIR)/src/lib/libarchive/* $@
	echo "LIBS = libarchive" > $@/target.mk

lib/mk/libarchive.mk:
	$(mirror_from_rep_dir)

LICENSE:
	cp $(PORT_DIR)/src/lib/libarchive/COPYING $@

