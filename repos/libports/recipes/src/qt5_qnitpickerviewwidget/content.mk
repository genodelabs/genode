MIRROR_FROM_REP_DIR := lib/mk/qt5_qnitpickerviewwidget.mk \
                       src/lib/qt5/qnitpickerviewwidget

content: $(MIRROR_FROM_REP_DIR) LICENSE src/lib/qt5_qnitpickerviewwidget/target.mk

$(MIRROR_FROM_REP_DIR):
	$(mirror_from_rep_dir)

src/lib/qt5_qnitpickerviewwidget/target.mk:
	mkdir -p $(dir $@)
	echo "LIBS = qt5_qnitpickerviewwidget" > $@

LICENSE:
	cp $(GENODE_DIR)/LICENSE $@
