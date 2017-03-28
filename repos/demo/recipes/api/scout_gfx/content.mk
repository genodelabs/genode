content: include/scout_gfx include/util src/lib/scout_gfx lib/mk/scout_gfx.mk LICENSE

include/scout_gfx include/util src/lib/scout_gfx lib/mk/scout_gfx.mk:
	$(mirror_from_rep_dir)

LICENSE:
	cp $(GENODE_DIR)/LICENSE $@

