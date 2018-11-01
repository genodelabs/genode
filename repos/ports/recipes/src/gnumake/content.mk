content: src/noux-pkg/make LICENSE

PORT_DIR := $(call port_dir,$(REP_DIR)/ports/make)

src/noux-pkg/make:
	mkdir -p $@
	cp -a $(PORT_DIR)/src/noux-pkg/make/* $@
	cp -a  $(REP_DIR)/src/noux-pkg/make/* $@

LICENSE:
	cp $(PORT_DIR)/src/noux-pkg/make/COPYING $@
