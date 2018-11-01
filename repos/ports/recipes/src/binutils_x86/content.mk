content: src/noux-pkg/binutils src/noux-pkg/binutils_x86 LICENSE

PORT_DIR := $(call port_dir,$(REP_DIR)/ports/binutils)

src/noux-pkg/binutils:
	mkdir -p $@
	cp -a $(PORT_DIR)/src/noux-pkg/binutils/* $@
	cp -a  $(REP_DIR)/src/noux-pkg/binutils/* $@

src/noux-pkg/binutils_x86:
	$(mirror_from_rep_dir)

LICENSE:
	cp $(PORT_DIR)/src/noux-pkg/binutils/COPYING $@
