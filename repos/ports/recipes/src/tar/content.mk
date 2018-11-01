content: src/noux-pkg/tar LICENSE

PORT_DIR := $(call port_dir,$(REP_DIR)/ports/tar)

src/noux-pkg/tar:
	mkdir -p $@
	cp -a $(PORT_DIR)/src/noux-pkg/tar/* $@
	cp -a  $(REP_DIR)/src/noux-pkg/tar/* $@

LICENSE:
	cp $(PORT_DIR)/src/noux-pkg/tar/COPYING $@
