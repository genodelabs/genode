content: include/polygon_gfx LICENSE

include/polygon_gfx:
	$(mirror_from_rep_dir)

LICENSE:
	cp $(GENODE_DIR)/LICENSE $@

