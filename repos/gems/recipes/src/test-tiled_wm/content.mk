MIRROR_FROM_REP_DIR := src/test/tiled_wm

content: $(MIRROR_FROM_REP_DIR) LICENSE

$(MIRROR_FROM_REP_DIR):
	$(mirror_from_rep_dir)

MIRROR_FROM_LIBPORTS := src/app/qt5/tmpl/target_defaults.inc \
                        src/app/qt5/tmpl/target_final.inc

content: $(MIRROR_FROM_LIBPORTS)

$(MIRROR_FROM_LIBPORTS):
	mkdir -p $(dir $@)
	cp -r $(GENODE_DIR)/repos/libports/$@ $(dir $@)

LICENSE:
	cp $(GENODE_DIR)/LICENSE $@
