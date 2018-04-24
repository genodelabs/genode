MIRROR_FROM_REP_DIR := src/test/qt5/qt_quick \
                       src/app/qt5/tmpl/target_defaults.inc \
                       src/app/qt5/tmpl/target_final.inc

content: $(MIRROR_FROM_REP_DIR) LICENSE

$(MIRROR_FROM_REP_DIR):
	$(mirror_from_rep_dir)

LICENSE:
	cp $(GENODE_DIR)/LICENSE $@
