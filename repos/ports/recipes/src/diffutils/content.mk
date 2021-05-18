content: src/noux-pkg/diffutils LICENSE

PORT_DIR := $(call port_dir,$(REP_DIR)/ports/diffutils)

src/noux-pkg/diffutils:
	mkdir -p $@
	cp -a $(PORT_DIR)/src/noux-pkg/diffutils/* $@
	cp -a  $(REP_DIR)/src/noux-pkg/diffutils/* $@

LICENSE:
	cp $(PORT_DIR)/src/noux-pkg/diffutils/COPYING $@

