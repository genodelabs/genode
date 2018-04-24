MIRROR_FROM_REP_DIR := lib/import/import-qt5_angle.mk \
                       lib/import/import-qt5_jscore.mk \
                       lib/import/import-qt5_webcore.mk \
                       lib/import/import-qt5_webkit.mk \
                       lib/import/import-qt5_webkitwidgets.mk \
                       lib/import/import-qt5_wtf.mk \
                       lib/import/import-qt5.inc

content: $(MIRROR_FROM_REP_DIR)

$(MIRROR_FROM_REP_DIR):
	$(mirror_from_rep_dir)

PORT_DIR := $(call port_dir,$(REP_DIR)/ports/qt5)

MIRROR_FROM_PORT_DIR := include/QtWebKit \
                        include/QtWebKitWidgets \
                        lib/symbols/qt5_angle \
                        lib/symbols/qt5_jscore \
                        lib/symbols/qt5_webcore \
                        lib/symbols/qt5_webkit \
                        lib/symbols/qt5_webkitwidgets \
                        lib/symbols/qt5_wtf

content: $(MIRROR_FROM_PORT_DIR)

$(MIRROR_FROM_PORT_DIR):
	mkdir -p $(dir $@)
	cp -r $(PORT_DIR)/$@ $(dir $@)

content: LICENSE

LICENSE:
	cp $(PORT_DIR)/src/lib/qt5/qt5/LICENSE.LGPLv3 $@
