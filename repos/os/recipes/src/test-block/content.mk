SRC_DIR = src/test/block
include $(GENODE_DIR)/repos/base/recipes/src/content.inc

MIRROR_FROM_REP_DIR := include/os

content: $(MIRROR_FROM_REP_DIR)

$(MIRROR_FROM_REP_DIR):
	$(mirror_from_rep_dir)
