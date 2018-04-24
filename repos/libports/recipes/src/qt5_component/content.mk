MIRROR_FROM_REP_DIR := lib/mk/qt5_component.mk \
                       src/lib/qt5/qt_component.cc

content: $(MIRROR_FROM_REP_DIR) LICENSE src/lib/qt5_component/target.mk

$(MIRROR_FROM_REP_DIR):
	$(mirror_from_rep_dir)

src/lib/qt5_component/target.mk:
	mkdir -p $(dir $@)
	echo "LIBS = qt5_component" > $@

LICENSE:
	cp $(GENODE_DIR)/LICENSE $@

