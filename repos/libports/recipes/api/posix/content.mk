MIRROR_FROM_REP_DIR := lib/symbols/posix lib/import/import-posix.mk

content: $(MIRROR_FROM_REP_DIR) LICENSE lib/mk/base.mk lib/mk/ldso-startup.mk

$(MIRROR_FROM_REP_DIR):
	$(mirror_from_rep_dir)

lib/mk/base.mk lib/mk/ldso-startup.mk:
	mkdir -p $(dir $@)
	touch $@

LICENSE:
	cp $(GENODE_DIR)/LICENSE $@

