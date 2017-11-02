content: src/noux-pkg/bash LICENSE

PORT_DIR := $(call port_dir,$(REP_DIR)/ports/bash)

src/noux-pkg/bash:
	mkdir -p $@
	cp -a $(PORT_DIR)/src/noux-pkg/bash/* $@
	cp -a  $(REP_DIR)/src/noux-pkg/bash/* $@

LICENSE:
	cp $(PORT_DIR)/src/noux-pkg/bash/COPYING $@

