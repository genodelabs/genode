BOARD = imx53_qsb_tz

include $(GENODE_DIR)/repos/base-hw/recipes/src/base-hw_content.inc

content: enable_board_spec
enable_board_spec: etc/specs.conf
	echo "SPECS += imx53_qsb trustzone" >> etc/specs.conf
