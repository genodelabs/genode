BINARIES := bash

INSTALL_TAR_CONTENT := $(addprefix bin/,$(BINARIES))

PKG_DIR = $(call select_from_ports,bash)/src/noux-pkg/bash

include $(REP_DIR)/src/noux-pkg/bash/target.inc
