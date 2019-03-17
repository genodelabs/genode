SRC_DIR = include/file_system src/server/vfs
include $(GENODE_DIR)/repos/base/recipes/src/content.inc

MIRROR_FROM_REP_DIR += $(addprefix include/ram_fs/,chunk.h param.h) \
                       lib/mk/vfs.mk src/lib/vfs

content: $(MIRROR_FROM_REP_DIR)

$(MIRROR_FROM_REP_DIR):
	$(mirror_from_rep_dir)
