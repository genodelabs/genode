MIRROR_FROM_REP_DIR := include/qt5/qgenodeviewwidget \
                       lib/import/import-qt5_qgenodeviewwidget.mk \
                       lib/symbols/qt5_qgenodeviewwidget

content: $(MIRROR_FROM_REP_DIR) LICENSE

$(MIRROR_FROM_REP_DIR):
	$(mirror_from_rep_dir)

LICENSE:
	cp $(GENODE_DIR)/LICENSE $@
