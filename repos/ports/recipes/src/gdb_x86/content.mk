content: src/noux-pkg/gdb LICENSE

PORT_DIR := $(call port_dir,$(REP_DIR)/ports/gdb)

src/noux-pkg/gdb:
	mkdir -p $@
	cp -a $(PORT_DIR)/src/noux-pkg/gdb/* $@
	cp -a  $(REP_DIR)/src/noux-pkg/gdb/* $@
	cp -a  $(REP_DIR)/src/noux-pkg/gdb_x86/* $@

LICENSE:
	cp $(PORT_DIR)/src/noux-pkg/gdb/COPYING $@
