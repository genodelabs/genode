BASH_SRC ?= $(addprefix src/noux-pkg/bash/,target.inc target.mk)

content: src/noux-pkg/bash LICENSE

PORT_DIR := $(call port_dir,$(REP_DIR)/ports/bash)

src/noux-pkg/bash:
	mkdir -p $@
	cp -a $(PORT_DIR)/src/noux-pkg/bash/* $@
	cp -a $(addprefix $(REP_DIR)/,$(BASH_SRC)) $@

LICENSE:
	cp $(PORT_DIR)/src/noux-pkg/bash/COPYING $@

