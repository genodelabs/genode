SRC_DIR := src/server/nitpicker src/app/pointer
include $(GENODE_DIR)/repos/base/recipes/src/content.inc

content: include/pointer include/report_rom

include/pointer:
	$(mirror_from_rep_dir)

include/report_rom:
	$(mirror_from_rep_dir)
