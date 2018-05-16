BINARIES := fsck.ext2 mkfs.ext2 resize2fs

INSTALL_TAR_CONTENT := $(addprefix bin/,$(BINARIES))

PKG_DIR = $(call select_from_ports,e2fsprogs)/src/noux-pkg/e2fsprogs

include $(REP_DIR)/src/noux-pkg/e2fsprogs/target.inc
