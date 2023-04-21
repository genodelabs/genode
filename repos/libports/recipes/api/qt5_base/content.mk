MIRROR_FROM_REP_DIR := lib/import/import-qt5_cmake.mk \
                       lib/import/import-qt5_qmake.mk

content: $(MIRROR_FROM_REP_DIR)

$(MIRROR_FROM_REP_DIR):
	$(mirror_from_rep_dir)

PORT_DIR := $(call port_dir,$(REP_DIR)/ports/qt5)
API_DIR  := $(PORT_DIR)/src/lib/qt5/genode/api

MIRROR_FROM_PORT_DIR := include \
                        mkspecs \
                        lib/cmake \

MIRROR_SYMBOLS := lib/symbols/libQt5Core \
                  lib/symbols/libQt5Gui \
                  lib/symbols/libQt5Network \
                  lib/symbols/libQt5PrintSupport \
                  lib/symbols/libQt5Sql \
                  lib/symbols/libQt5Test \
                  lib/symbols/libQt5Widgets \
                  lib/symbols/libQt5Xml \
                  lib/symbols/libqgenode


content: $(MIRROR_FROM_PORT_DIR)

$(MIRROR_FROM_PORT_DIR):
	cp -r $(API_DIR)/$@ $@

content: $(MIRROR_SYMBOLS)

$(MIRROR_SYMBOLS):
	mkdir -p lib/symbols
	cp $(API_DIR)/$@ lib/symbols


content: LICENSE

LICENSE:
	cp $(PORT_DIR)/src/lib/qt5/LICENSE.LGPLv3 $@
