# keep 'vim/target.inc' but not 'vim/target.mk'
VIM_SRC := src/noux-pkg/vim/target.inc

include $(REP_DIR)/recipes/src/vim/content.mk

content: src/noux-pkg/vim-minimal

src/noux-pkg/vim-minimal:
	$(mirror_from_rep_dir)
