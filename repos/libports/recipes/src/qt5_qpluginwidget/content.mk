MIRROR_FROM_REP_DIR := lib/mk/qt5_qpluginwidget.mk \
                       src/lib/qt5/qpluginwidget

content: $(MIRROR_FROM_REP_DIR) LICENSE src/lib/qt5_qpluginwidget/target.mk

$(MIRROR_FROM_REP_DIR):
	$(mirror_from_rep_dir)

src/lib/qt5_qpluginwidget/target.mk:
	mkdir -p $(dir $@)
	echo "LIBS = qt5_qpluginwidget" > $@

LICENSE:
	cp $(GENODE_DIR)/LICENSE $@
