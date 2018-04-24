MIRROR_FROM_REP_DIR := include/qt5/qnitpickerviewwidget \
                       lib/import/import-qt5_qnitpickerviewwidget.mk \
                       lib/symbols/qt5_qnitpickerviewwidget

content: $(MIRROR_FROM_REP_DIR) LICENSE

$(MIRROR_FROM_REP_DIR):
	$(mirror_from_rep_dir)

LICENSE:
	cp $(GENODE_DIR)/LICENSE $@
