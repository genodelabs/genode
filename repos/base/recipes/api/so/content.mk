content: lib/mk/ldso-startup.mk LICENSE

lib/mk/ldso-startup.mk:
	$(mirror_from_rep_dir)

LICENSE:
	cp $(GENODE_DIR)/LICENSE $@
