content: include/nitpicker_gfx LICENSE

include/nitpicker_gfx:
	$(mirror_from_rep_dir)

LICENSE:
	cp $(GENODE_DIR)/LICENSE $@

