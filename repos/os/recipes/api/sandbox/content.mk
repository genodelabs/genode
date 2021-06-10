content: include/sandbox lib/symbols/sandbox LICENSE

include/sandbox lib/symbols/sandbox:
	$(mirror_from_rep_dir)

LICENSE:
	cp $(GENODE_DIR)/LICENSE $@

