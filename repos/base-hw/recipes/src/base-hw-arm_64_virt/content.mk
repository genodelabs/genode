BOARD = arm_64_virt

include $(GENODE_DIR)/repos/base-hw/recipes/src/base-hw_content.inc

content: enable_board_spec
enable_board_spec: etc/specs.conf
	echo "SPECS += arm_64_virt" >> etc/specs.conf

