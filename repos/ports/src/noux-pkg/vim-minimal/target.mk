INSTALL_TAR_CONTENT := bin/vim

PKG_DIR = $(call select_from_ports,vim)/src/noux-pkg/vim

include $(REP_DIR)/src/noux-pkg/vim/target.inc
