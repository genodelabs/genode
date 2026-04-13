SRC_DIR := src/app/pc_discover

include $(GENODE_DIR)/repos/base/recipes/src/content.inc

content: $(MIRROR_FROM_REP_DIR)

$(MIRROR_FROM_REP_DIR):
	$(mirror_from_rep_dir)
