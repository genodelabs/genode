content: src/noux-pkg/grep LICENSE

PORT_DIR := $(call port_dir,$(REP_DIR)/ports/grep)

src/noux-pkg/grep:
	mkdir -p $@
	cp -a $(PORT_DIR)/src/noux-pkg/grep/* $@
	cp -a  $(REP_DIR)/src/noux-pkg/grep/* $@

LICENSE:
	cp $(PORT_DIR)/src/noux-pkg/grep/COPYING $@

