MIRROR_FROM_REP_DIR := lib/symbols/compat-libc

content: $(MIRROR_FROM_REP_DIR) LICENSE

$(MIRROR_FROM_REP_DIR):
	$(mirror_from_rep_dir)

content: lib/symbol.map

lib/symbol.map:
	cp $(REP_DIR)/src/lib/compat-libc/symbol.map $@


LICENSE:
	cp $(GENODE_DIR)/LICENSE $@

