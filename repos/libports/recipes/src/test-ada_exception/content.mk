#MIRROR_FROM_REP_DIR := \
#	lib/mk/test-ada.mk \
#	lib/import/import-test-ada.mk
#
#content: $(MIRROR_FROM_REP_DIR)
#
#$(MIRROR_FROM_REP_DIR):
#	$(mirror_from_rep_dir)


SRC_DIR = src/test/ada_exception

include $(GENODE_DIR)/repos/base/recipes/src/content.inc
