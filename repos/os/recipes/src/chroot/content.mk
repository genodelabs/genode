content: include/file_system

SRC_DIR = src/server/chroot
include $(GENODE_DIR)/repos/base/recipes/src/content.inc

include/file_system:
	mkdir -p $@
	cp $(GENODE_DIR)/repos/os/include/file_system/* $@
