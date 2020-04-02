content: lib/mk/ldso_so_support.mk src/lib/ldso/so_support.c LICENSE

lib/mk/ldso_so_support.mk src/lib/ldso/so_support.c:
	$(mirror_from_rep_dir)

LICENSE:
	cp $(GENODE_DIR)/LICENSE $@
