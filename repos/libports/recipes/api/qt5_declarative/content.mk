PORT_DIR := $(call port_dir,$(REP_DIR)/ports/qt5)

MIRROR_SYMBOLS := src/lib/qt5/genode/api/lib/symbols/libQt5Qml \
                  src/lib/qt5/genode/api/lib/symbols/libQt5QmlModels \
                  src/lib/qt5/genode/api/lib/symbols/libQt5Quick \
                  src/lib/qt5/genode/api/lib/symbols/libQt5QuickWidgets

content: $(MIRROR_SYMBOLS)

$(MIRROR_SYMBOLS):
	mkdir -p lib/symbols
	cp $(PORT_DIR)/$@ lib/symbols

content: LICENSE

LICENSE:
	cp $(PORT_DIR)/src/lib/qt5/LICENSE.LGPLv3 $@

CMAKE_MODULES := FindQt5Qml.cmake \
                 FindQt5QmlModels.cmake \
                 FindQt5Quick.cmake \
                 FindQt5QuickWidgets.cmake

content: $(CMAKE_MODULES)

$(CMAKE_MODULES): Find%.cmake:
	echo 'set($*_FOUND True)' | sed -r 's/\w+_FOUND/\U&/' > $@
