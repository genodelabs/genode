MIRROR_FROM_REP_DIR := \
	lib/mk/test-spark.mk \
	lib/import/import-test-spark.mk

content: $(MIRROR_FROM_REP_DIR)

$(MIRROR_FROM_REP_DIR):
	$(mirror_from_rep_dir)


SRC_DIR = src/test/spark

include $(GENODE_DIR)/repos/base/recipes/src/content.inc
