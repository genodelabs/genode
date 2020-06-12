MIRROR_FROM_REP_DIR := lib/mk/qt5_qgenodeviewwidget.mk \
                       src/lib/qt5/qgenodeviewwidget

content: $(MIRROR_FROM_REP_DIR) LICENSE src/lib/qt5_qgenodeviewwidget/target.mk

$(MIRROR_FROM_REP_DIR):
	$(mirror_from_rep_dir)

src/lib/qt5_qgenodeviewwidget/target.mk:
	mkdir -p $(dir $@)
	echo "LIBS = qt5_qgenodeviewwidget" > $@

LICENSE:
	cp $(GENODE_DIR)/LICENSE $@
