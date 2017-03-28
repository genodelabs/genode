SRC_DIR = src/drivers/platform
include $(GENODE_DIR)/repos/base/recipes/src/content.inc

content: include/init/child_policy.h

include/init/child_policy.h:
	$(mirror_from_rep_dir)
