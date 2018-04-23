content: include/vfs lib/symbols/vfs LICENSE

include/vfs lib/symbols/vfs:
	$(mirror_from_rep_dir)

LICENSE:
	cp $(GENODE_DIR)/LICENSE $@

