MIRROR_FROM_REP_DIR := lib/symbols/libc_noux mk/noux.mk mk/gnu_build.mk

content:$(MIRROR_FROM_REP_DIR) LICENSE

$(MIRROR_FROM_REP_DIR):
	$(mirror_from_rep_dir)

LICENSE:
	cp $(GENODE_DIR)/LICENSE $@

