SRC_DIR := src/app/backdrop
include $(GENODE_DIR)/repos/base/recipes/src/content.inc

content: src/lib/file lib/mk/file.mk

src/lib/file lib/mk/file.mk:
	$(mirror_from_rep_dir)
