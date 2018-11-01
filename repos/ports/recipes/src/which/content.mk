content: src/noux-pkg/which LICENSE

PORT_DIR := $(call port_dir,$(REP_DIR)/ports/which)

src/noux-pkg/which:
	mkdir -p $@
	cp -a $(PORT_DIR)/src/noux-pkg/which/* $@
	cp -a  $(REP_DIR)/src/noux-pkg/which/* $@

LICENSE:
	cp $(PORT_DIR)/src/noux-pkg/which/COPYING $@
