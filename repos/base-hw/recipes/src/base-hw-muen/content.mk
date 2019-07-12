BOARD = muen

include $(GENODE_DIR)/repos/base-hw/recipes/src/base-hw_content.inc

content: enable_board_spec
enable_board_spec: etc/specs.conf
	echo "SPECS += muen" >> etc/specs.conf

content: src/acpi/target.mk
src/acpi/target.mk:
	mkdir $(dir $@)
	cp $(REP_DIR)/recipes/src/base-hw-muen/acpi_target_mk $@

