MIRROR_FROM_REP_DIR := src/qt5/qgenodeviewwidget/target.mk \
                       src/lib/qgenodeviewwidget

content: $(MIRROR_FROM_REP_DIR) LICENSE

$(MIRROR_FROM_REP_DIR):
	$(mirror_from_rep_dir)

LICENSE:
	cp $(GENODE_DIR)/LICENSE $@
