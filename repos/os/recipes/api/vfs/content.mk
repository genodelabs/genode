content: include/vfs include/ram_fs/chunk.h lib/mk/vfs.mk src/lib/vfs LICENSE

include/vfs include/ram_fs/chunk.h lib/mk/vfs.mk src/lib/vfs:
	$(mirror_from_rep_dir)

LICENSE:
	cp $(GENODE_DIR)/LICENSE $@

