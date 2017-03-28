content: include/blit src/lib/blit lib/mk/blit.mk LICENSE

src/lib/blit include/blit lib/mk/blit.mk:
	$(mirror_from_rep_dir)

LICENSE:
	cp $(GENODE_DIR)/LICENSE $@

