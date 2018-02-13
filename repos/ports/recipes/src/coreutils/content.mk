# allow coreutils-minimal to exclude 'coreutils/target.mk'
COREUTILS_SRC ?= $(addprefix src/noux-pkg/coreutils/,target.inc target.mk)

content: src/noux-pkg/coreutils LICENSE

PORT_DIR := $(call port_dir,$(REP_DIR)/ports/coreutils)

src/noux-pkg/coreutils:
	mkdir -p $@
	cp -a $(PORT_DIR)/src/noux-pkg/coreutils/* $@
	cp -a $(addprefix $(REP_DIR)/,$(COREUTILS_SRC)) $@

LICENSE:
	cp $(PORT_DIR)/src/noux-pkg/coreutils/COPYING $@

