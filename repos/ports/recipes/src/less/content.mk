content: src/noux-pkg/less LICENSE

PORT_DIR := $(call port_dir,$(REP_DIR)/ports/less)

src/noux-pkg/less:
	mkdir -p $@
	cp -a $(PORT_DIR)/src/noux-pkg/less/* $@
	cp -a  $(REP_DIR)/src/noux-pkg/less/* $@

LICENSE:
	cp $(PORT_DIR)/src/noux-pkg/less/COPYING $@

