content: src/noux-pkg/sed LICENSE

PORT_DIR := $(call port_dir,$(REP_DIR)/ports/sed)

src/noux-pkg/sed:
	mkdir -p $@
	cp -a $(PORT_DIR)/src/noux-pkg/sed/* $@
	cp -a  $(REP_DIR)/src/noux-pkg/sed/* $@

LICENSE:
	cp $(PORT_DIR)/src/noux-pkg/sed/COPYING $@
