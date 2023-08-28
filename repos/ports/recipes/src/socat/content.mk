content: src/noux-pkg/socat LICENSE

PORT_DIR := $(call port_dir,$(REP_DIR)/ports/socat)

src/noux-pkg/socat:
	mkdir -p $@
	cp -a $(PORT_DIR)/src/noux-pkg/socat/* $@
	cp -a  $(REP_DIR)/src/noux-pkg/socat/* $@

LICENSE:
	cp $(PORT_DIR)/src/noux-pkg/socat/COPYING $@

