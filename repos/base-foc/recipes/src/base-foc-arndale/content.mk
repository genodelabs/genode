RECIPE_DIR := $(REP_DIR)/recipes/src/base-foc-arndale

include $(GENODE_DIR)/repos/base-foc/recipes/src/base-foc_content.inc

content: enable_board_spec
enable_board_spec: etc/specs.conf
	echo "SPECS += arndale" >> etc/specs.conf
