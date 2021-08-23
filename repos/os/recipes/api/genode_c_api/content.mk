MIRROR_FROM_REP_DIR := include/genode_c_api \
                       src/lib/genode_c_api

content: $(MIRROR_FROM_REP_DIR) LICENSE

$(MIRROR_FROM_REP_DIR):
	$(mirror_from_rep_dir)

LICENSE:
	cp $(GENODE_DIR)/LICENSE $@
