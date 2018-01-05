SRC_DIR = src/server/clipboard
include $(GENODE_DIR)/repos/base/recipes/src/content.inc

content: include/report_rom

include/report_rom:
	$(mirror_from_rep_dir)
