SRC_DIR := src/app/file_vault

content: $(MIRROR_FROM_REP_DIR)

$(MIRROR_FROM_REP_DIR):
	$(mirror_from_rep_dir)

include $(GENODE_DIR)/repos/base/recipes/src/content.inc
