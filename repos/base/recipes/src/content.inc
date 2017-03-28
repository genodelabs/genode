content: $(SRC_DIR) LICENSE

$(SRC_DIR):
	mkdir -p $@
	cp -r $(REP_DIR)/$@/* $@/

LICENSE:
	cp $(GENODE_DIR)/LICENSE $@
