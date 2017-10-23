content: src/noux-pkg/findutils LICENSE

PORT_DIR := $(call port_dir,$(REP_DIR)/ports/findutils)

src/noux-pkg/findutils:
	mkdir -p $@
	cp -a $(PORT_DIR)/src/noux-pkg/findutils/* $@
	cp -a  $(REP_DIR)/src/noux-pkg/findutils/* $@

LICENSE:
	cp $(PORT_DIR)/src/noux-pkg/findutils/COPYING $@

