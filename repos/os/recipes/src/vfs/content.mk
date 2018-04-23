SRC_DIR = include/file_system src/server/vfs
include $(GENODE_DIR)/repos/base/recipes/src/content.inc


content: include/ram_fs/chunk.h lib/mk/vfs.mk src/lib/vfs

include/vfs include/ram_fs/chunk.h lib/mk/vfs.mk src/lib/vfs:
	$(mirror_from_rep_dir)
