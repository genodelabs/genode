SRC_DIR = src/lib/dialog
include $(GENODE_DIR)/repos/base/recipes/src/content.inc

content: lib/mk/dialog.mk

lib/mk/dialog.mk:
	$(mirror_from_rep_dir)
