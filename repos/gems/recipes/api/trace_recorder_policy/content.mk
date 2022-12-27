content: include/trace_recorder_policy LICENSE

include/trace_recorder_policy:
	$(mirror_from_rep_dir)

LICENSE:
	cp $(GENODE_DIR)/LICENSE $@

