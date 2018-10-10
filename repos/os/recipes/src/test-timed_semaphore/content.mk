SRC_DIR = src/test/timed_semaphore src/lib/timed_semaphore
include $(GENODE_DIR)/repos/base/recipes/src/content.inc

MIRROR_FROM_REP_DIR := lib/mk/timed_semaphore.mk

content: $(MIRROR_FROM_REP_DIR)

$(MIRROR_FROM_REP_DIR):
	$(mirror_from_rep_dir)
