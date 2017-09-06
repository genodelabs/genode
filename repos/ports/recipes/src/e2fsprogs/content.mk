content: src/noux-pkg/e2fsprogs LICENSE

PORT_DIR := $(call port_dir,$(REP_DIR)/ports/e2fsprogs)

src/noux-pkg/e2fsprogs:
	mkdir -p $@
	cp -r $(PORT_DIR)/src/noux-pkg/e2fsprogs/* $@
	cp -r  $(REP_DIR)/src/noux-pkg/e2fsprogs/* $@

LICENSE:
	cp $(PORT_DIR)/src/noux-pkg/e2fsprogs/COPYING $@

