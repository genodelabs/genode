SRC_DIR := src/app/file_vault

MIRROR_FROM_REP_DIR := \
	src/lib/tresor/include/tresor/types.h \
	src/lib/tresor/include/tresor/math.h \
	src/lib/tresor/include/tresor/verbosity.h

content: $(MIRROR_FROM_REP_DIR)

$(MIRROR_FROM_REP_DIR):
	$(mirror_from_rep_dir)

include $(GENODE_DIR)/repos/base/recipes/src/content.inc
