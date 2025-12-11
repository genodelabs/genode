INSTALL_TAR_CONTENT := bin/vim \
                       share/vim/vim73/syntax/syntax.vim \
                       share/vim/vim73/colors/default.vim \
                       share/vim/vim73/syntax/synload.vim \
                       share/vim/vim73/syntax/syncolor.vim \
                       share/vim/vim73/filetype.vim

PKG_DIR := $(call select_from_ports,vim)/src/noux-pkg/vim

include $(REP_DIR)/src/noux-pkg/vim/target.inc
