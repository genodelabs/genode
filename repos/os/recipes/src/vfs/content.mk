SRC_DIR = include/file_system src/server/vfs
include $(GENODE_DIR)/repos/base/recipes/src/content.inc

LIB_SRC := $(notdir $(wildcard $(REP_DIR)/src/lib/vfs/*.h $(REP_DIR)/src/lib/vfs/*.cc))

MIRROR_FROM_REP_DIR += $(addprefix include/ram_fs/,chunk.h param.h) \
                       $(addprefix src/lib/vfs/,$(LIB_SRC)) \
                       lib/mk/vfs.mk src/lib/vfs

content: $(MIRROR_FROM_REP_DIR)

$(MIRROR_FROM_REP_DIR):
	$(mirror_from_rep_dir)
