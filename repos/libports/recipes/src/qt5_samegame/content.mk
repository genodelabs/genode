MIRROR_FROM_REP_DIR := src/app/qt5/examples/samegame \
                       src/app/qt5/tmpl/target_defaults.inc \
                       src/app/qt5/tmpl/target_final.inc

content: $(MIRROR_FROM_REP_DIR) LICENSE

$(MIRROR_FROM_REP_DIR):
	$(mirror_from_rep_dir)

LICENSE:
	cp $(GENODE_DIR)/LICENSE $@
