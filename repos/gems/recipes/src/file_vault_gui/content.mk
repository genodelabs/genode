SRC_DIR := src/app/file_vault_gui
MIRROR_FROM_REP_DIR := \
	lib/mk/dialog.mk src/lib/dialog include/dialog src/app/file_vault/include \
	src/lib/tresor/include/tresor/types.h \
	src/lib/tresor/include/tresor/assertion.h \
	src/lib/tresor/include/tresor/verbosity.h \
	src/lib/tresor/include/tresor/math.h

content: $(MIRROR_FROM_REP_DIR)

$(MIRROR_FROM_REP_DIR):
	$(mirror_from_rep_dir)

include $(GENODE_DIR)/repos/base/recipes/src/content.inc
