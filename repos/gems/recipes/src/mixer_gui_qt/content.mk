MIRROR_FROM_REP_DIR := src/app/mixer_gui_qt

content: $(MIRROR_FROM_REP_DIR) LICENSE

$(MIRROR_FROM_REP_DIR):
	$(mirror_from_rep_dir)

MIRROR_FROM_LIBPORTS := src/app/qt5/tmpl/target_defaults.inc \
                        src/app/qt5/tmpl/target_final.inc

content: $(MIRROR_FROM_LIBPORTS)

$(MIRROR_FROM_LIBPORTS):
	mkdir -p $(dir $@)
	cp -r $(GENODE_DIR)/repos/libports/$@ $(dir $@)

MIRROR_FROM_OS := include/mixer

content: $(MIRROR_FROM_OS)

$(MIRROR_FROM_OS):
	mkdir -p $(dir $@)
	cp -r $(GENODE_DIR)/repos/os/$@ $(dir $@)

LICENSE:
	cp $(GENODE_DIR)/LICENSE $@
