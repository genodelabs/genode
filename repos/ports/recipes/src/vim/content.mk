content: src/noux-pkg/vim LICENSE

PORT_DIR := $(call port_dir,$(REP_DIR)/ports/vim)

src/noux-pkg/vim:
	mkdir -p $@
	cp -r $(PORT_DIR)/src/noux-pkg/vim/* $@
	cp -r  $(REP_DIR)/src/noux-pkg/vim/* $@

LICENSE:
	cp $(PORT_DIR)/src/noux-pkg/vim/runtime/doc/uganda.txt $@

