MIRROR_FROM_REP_DIR := lib/mk/libuvc.mk lib/import/import-libuvc.mk

content: src/lib/libuvc include LICENSE $(MIRROR_FROM_REP_DIR)

PORT_DIR := $(call port_dir,$(REP_DIR)/ports/libuvc)

src/lib/libuvc:
	mkdir -p $(dir $@)
	cp -r $(PORT_DIR)/src/lib/libuvc $@
	rm -rf $@/.git

include:
	mkdir -p $@
	cp -a $(PORT_DIR)/include/* $@


$(MIRROR_FROM_REP_DIR):
	$(mirror_from_rep_dir)

LICENSE:
	cp $(PORT_DIR)/src/lib/libuvc/LICENSE.txt $@
