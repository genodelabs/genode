SRC_DIR := src/app/vbox_pointer
include $(GENODE_DIR)/repos/base/recipes/src/content.inc

content: include/vbox_pointer

include/vbox_pointer:
	$(mirror_from_rep_dir)
