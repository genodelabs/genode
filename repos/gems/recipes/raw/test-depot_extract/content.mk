content: public.tar installation download pubkey

public.tar installation download:
	cp $(REP_DIR)/recipes/raw/test-depot_extract/$@ $@

pubkey:
	cp $(GENODE_DIR)/repos/gems/sculpt/depot/nfeske/pubkey $@
