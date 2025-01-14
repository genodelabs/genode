PORT_DIR := $(call port_dir,$(REP_DIR)/ports/qt6_api)

MIRROR_LIB_SYMBOLS := libQt6Svg \
                      libQt6SvgWidgets

content: $(MIRROR_LIB_SYMBOLS)

$(MIRROR_LIB_SYMBOLS):
	mkdir -p lib/symbols
	cp $(PORT_DIR)/src/lib/qt6_api/lib/symbols/$@ lib/symbols/

content: LICENSE

LICENSE:
	cp $(PORT_DIR)/src/lib/qt6_api/LICENSE.LGPL3 $@
