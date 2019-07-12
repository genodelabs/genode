BOARD = imx8q_evk

include $(GENODE_DIR)/repos/base-hw/recipes/src/base-hw_content.inc

content: enable_board_spec
enable_board_spec: etc/specs.conf
	echo "SPECS += imx8q_evk" >> etc/specs.conf
