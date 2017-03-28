FROM_BASE_HW := etc include

content: $(FROM_BASE_HW) LICENSE

$(FROM_BASE_HW):
	$(mirror_from_rep_dir)

LICENSE:
	cp $(GENODE_DIR)/LICENSE $@
