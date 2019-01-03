#
# Common content rules shared among all base-<kernel> source archives
#

content: src/include src/core src/lib src/timer lib/mk LICENSE

src/include src/core src/lib src/timer lib/mk:
	mkdir -p $@
	cp -r $(GENODE_DIR)/repos/base/$@/* $@
	cp -r $(REP_DIR)/$@/* $@

LICENSE:
	cp $(GENODE_DIR)/LICENSE $@

content: etc/specs.conf

etc/specs.conf:
	$(mirror_from_rep_dir)
