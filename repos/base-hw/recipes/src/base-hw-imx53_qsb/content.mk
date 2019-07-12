BOARD = imx53_qsb

include $(GENODE_DIR)/repos/base-hw/recipes/src/base-hw_content.inc

content: enable_board_spec
enable_board_spec: etc/specs.conf
	echo "SPECS += imx53_qsb" >> etc/specs.conf
