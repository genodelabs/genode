MIRRORED_FROM_REP_DIR := include/vm_session

content: $(MIRRORED_FROM_REP_DIR) LICENSE

$(MIRRORED_FROM_REP_DIR):
	$(mirror_from_rep_dir)

LICENSE:
	cp $(GENODE_DIR)/LICENSE $@
