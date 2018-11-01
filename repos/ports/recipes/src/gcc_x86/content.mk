content: src/noux-pkg/gcc LICENSE

PORT_DIR := $(call port_dir,$(REP_DIR)/ports/gcc)

src/noux-pkg/gcc:
	mkdir -p $@
	cp -a $(PORT_DIR)/src/noux-pkg/gcc/* $@
	cp -a  $(REP_DIR)/src/noux-pkg/gcc/* $@
	cp -a  $(REP_DIR)/src/noux-pkg/gcc_x86/* $@

LICENSE:
	cp $(PORT_DIR)/src/noux-pkg/gcc/COPYING $@
