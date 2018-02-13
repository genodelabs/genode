# allow vim-minimal to exclude 'vim/target.mk'
VIM_SRC ?= $(addprefix src/noux-pkg/vim/,target.inc target.mk)

content: src/noux-pkg/vim LICENSE

PORT_DIR := $(call port_dir,$(REP_DIR)/ports/vim)

src/noux-pkg/vim:
	mkdir -p $@
	cp -a $(PORT_DIR)/src/noux-pkg/vim/* $@
	cp -a $(addprefix $(REP_DIR)/,$(VIM_SRC)) $@

LICENSE:
	cp $(PORT_DIR)/src/noux-pkg/vim/runtime/doc/uganda.txt $@

