SRC_DIR := src/app/text_area

MIRROR_FROM_REP_DIR := lib/mk/dialog.mk src/lib/dialog include/dialog

content: $(MIRROR_FROM_REP_DIR)

$(MIRROR_FROM_REP_DIR):
	$(mirror_from_rep_dir)

include $(GENODE_DIR)/repos/base/recipes/src/content.inc
