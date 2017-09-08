content: include/nano3d LICENSE

include/nano3d:
	$(mirror_from_rep_dir)

LICENSE:
	cp $(GENODE_DIR)/LICENSE $@

