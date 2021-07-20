content: include/genode_c_api LICENSE

include/genode_c_api:
	$(mirror_from_rep_dir)

LICENSE:
	cp $(GENODE_DIR)/LICENSE $@
