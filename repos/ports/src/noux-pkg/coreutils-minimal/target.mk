BINARIES := cat cp ls mkdir mv rm rmdir sort tail md5sum

INSTALL_TAR_CONTENT := $(addprefix bin/,$(BINARIES))

PKG_DIR = $(call select_from_ports,coreutils)/src/noux-pkg/coreutils

include $(REP_DIR)/src/noux-pkg/coreutils/target.inc
