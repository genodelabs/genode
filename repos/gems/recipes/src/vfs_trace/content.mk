MIRROR_FROM_REP_DIR := lib/mk/vfs_trace.mk src/lib/vfs/trace

content: $(MIRROR_FROM_REP_DIR) LICENSE

$(MIRROR_FROM_REP_DIR):
	$(mirror_from_rep_dir)

LICENSE:
	cp $(GENODE_DIR)/LICENSE $@
