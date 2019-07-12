BOARD = imx7d_sabre

include $(GENODE_DIR)/repos/base-hw/recipes/src/base-hw_content.inc

content: enable_board_spec
enable_board_spec: etc/specs.conf
	echo "SPECS += imx7d_sabre" >> etc/specs.conf
