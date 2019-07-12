BOARD = nit6_solox

include $(GENODE_DIR)/repos/base-hw/recipes/src/base-hw_content.inc

content: enable_board_spec
enable_board_spec: etc/specs.conf
	echo "SPECS += nit6_solox" >> etc/specs.conf
