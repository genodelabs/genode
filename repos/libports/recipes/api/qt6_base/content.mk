MIRROR_FROM_REP_DIR := lib/import/import-qt6.inc \
                       lib/import/import-qt6_cmake.mk \
                       lib/import/import-qt6_qmake.mk \
                       lib/mk/qt6_cmake.mk \
                       lib/mk/qt6_qmake.mk

content: $(MIRROR_FROM_REP_DIR)

$(MIRROR_FROM_REP_DIR):
	$(mirror_from_rep_dir)

PORT_DIR := $(call port_dir,$(REP_DIR)/ports/qt6)
API_DIR  := $(PORT_DIR)/src/lib/qt6/genode/api

MIRROR_FROM_PORT_DIR := include \
                        metatypes \
                        mkspecs \
                        lib/cmake \

MIRROR_SYMBOLS := lib/symbols/libQt6Concurrent \
                  lib/symbols/libQt6Core \
                  lib/symbols/libQt6Gui \
                  lib/symbols/libQt6Network \
                  lib/symbols/libQt6OpenGL \
                  lib/symbols/libQt6OpenGLWidgets \
                  lib/symbols/libQt6PrintSupport \
                  lib/symbols/libQt6Sql \
                  lib/symbols/libQt6Test \
                  lib/symbols/libQt6Widgets \
                  lib/symbols/libQt6Xml \
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
	cp $(PORT_DIR)/src/lib/qt6/LICENSE.LGPL3 $@
