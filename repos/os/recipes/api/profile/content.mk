MIRROR_FROM_REP_DIR := include/profile \
                       lib/import/import-profile.mk \
                       src/lib/profile \
                       lib/mk/profile.mk

content: $(MIRROR_FROM_REP_DIR) LICENSE

$(MIRROR_FROM_REP_DIR):
	$(mirror_from_rep_dir)

LICENSE:
	cp $(GENODE_DIR)/LICENSE $@

