SRC_DIR := src/app/depot_query

include $(GENODE_DIR)/repos/base/recipes/src/content.inc

MIRROR_FROM_REP_DIR := include/depot include/gems/vfs.h

content: $(MIRROR_FROM_REP_DIR)

$(MIRROR_FROM_REP_DIR):
	$(mirror_from_rep_dir)
