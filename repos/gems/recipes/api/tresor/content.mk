MIRROR_FROM_REP_DIR := \
	src/lib/tresor/include \
	lib/import/import-tresor.mk

content: $(MIRROR_FROM_REP_DIR) LICENSE

$(MIRROR_FROM_REP_DIR):
	$(mirror_from_rep_dir)

LICENSE:
	cp $(GENODE_DIR)/LICENSE $@
