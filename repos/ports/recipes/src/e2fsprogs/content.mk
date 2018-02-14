E2FSPROGS_SRC ?= $(addprefix src/noux-pkg/e2fsprogs/,target.inc target.mk)

content: src/noux-pkg/e2fsprogs LICENSE

PORT_DIR := $(call port_dir,$(REP_DIR)/ports/e2fsprogs)

src/noux-pkg/e2fsprogs:
	mkdir -p $@
	cp -a $(PORT_DIR)/src/noux-pkg/e2fsprogs/* $@
	cp -a $(addprefix $(REP_DIR)/,$(E2FSPROGS_SRC)) $@

LICENSE:
	cp $(PORT_DIR)/src/noux-pkg/e2fsprogs/COPYING $@

