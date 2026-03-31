content: public.tar install download pubkey

public.tar install download:
	cp $(REP_DIR)/recipes/raw/test-depot_extract/$@ $@

pubkey:
	cp $(GENODE_DIR)/repos/gems/sculpt/depot/nfeske/pubkey $@
