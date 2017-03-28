content: include/gems src/lib/file lib/mk/file.mk LICENSE

include/gems src/lib/file lib/mk/file.mk:
	$(mirror_from_rep_dir)

LICENSE:
	cp $(GENODE_DIR)/LICENSE $@

