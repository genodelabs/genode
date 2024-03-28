MIRROR_FROM_REP_DIR := lib/mk/vfs_legacy_oss.mk src/lib/vfs/legacy_oss

content: $(MIRROR_FROM_REP_DIR) LICENSE

$(MIRROR_FROM_REP_DIR):
	$(mirror_from_rep_dir)

LICENSE:
	cp $(GENODE_DIR)/LICENSE $@
