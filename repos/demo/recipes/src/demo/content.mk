content: doc include lib src include/init/child_policy.h LICENSE

doc include lib src:
	$(mirror_from_rep_dir)

include/init/child_policy.h:
	mkdir -p $(dir $@)
	cp $(GENODE_DIR)/repos/os/$@ $@

LICENSE:
	cp $(GENODE_DIR)/LICENSE $@
