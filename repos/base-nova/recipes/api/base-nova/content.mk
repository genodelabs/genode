FROM_BASE_NOVA := etc include

content: $(FROM_BASE_NOVA) LICENSE

$(FROM_BASE_NOVA):
	$(mirror_from_rep_dir)

LICENSE:
	cp $(GENODE_DIR)/LICENSE $@
