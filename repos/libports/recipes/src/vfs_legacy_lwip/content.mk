MIRROR_FROM_REP_DIR := src/lib/vfs/legacy_lwip lib/mk/vfs_legacy_lwip.mk

content: $(MIRROR_FROM_REP_DIR) LICENSE

$(MIRROR_FROM_REP_DIR):
	$(mirror_from_rep_dir)

LICENSE:
	cp $(GENODE_DIR)/LICENSE $@
