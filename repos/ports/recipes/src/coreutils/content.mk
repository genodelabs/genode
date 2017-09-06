content: src/noux-pkg/coreutils LICENSE

PORT_DIR := $(call port_dir,$(REP_DIR)/ports/coreutils)

src/noux-pkg/coreutils:
	mkdir -p $@
	cp -r $(PORT_DIR)/src/noux-pkg/coreutils/* $@
	cp -r  $(REP_DIR)/src/noux-pkg/coreutils/* $@

LICENSE:
	cp $(PORT_DIR)/src/noux-pkg/coreutils/COPYING $@

