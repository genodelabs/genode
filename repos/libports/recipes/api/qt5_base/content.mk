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

CMAKE_MODULES := FindQt5Core.cmake \
                 FindQt5Gui.cmake \
                 FindQt5Network.cmake \
                 FindQt5PrintSupport.cmake \
                 FindQt5Sql.cmake \
                 FindQt5Test.cmake \
                 FindQt5Widgets.cmake \
                 FindQt5Xml.cmake

content: $(CMAKE_MODULES)

$(CMAKE_MODULES): Find%.cmake:
	echo 'set($*_FOUND True)' | sed -r 's/\w+_FOUND/\U&/' > $@
