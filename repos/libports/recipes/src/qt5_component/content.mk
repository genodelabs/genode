MIRROR_FROM_REP_DIR := lib/mk/qt5_component.mk \
                       src/lib/qt5_component/qt_component.cc

content: $(MIRROR_FROM_REP_DIR) LICENSE

$(MIRROR_FROM_REP_DIR):
	$(mirror_from_rep_dir)

LICENSE:
	cp $(GENODE_DIR)/LICENSE $@
