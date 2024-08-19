PORT_DIR := $(call port_dir,$(REP_DIR)/ports/qt6)

MIRROR_LIB_SYMBOLS := libQt6Svg

content: $(MIRROR_LIB_SYMBOLS)

$(MIRROR_LIB_SYMBOLS):
	mkdir -p lib/symbols
	cp $(PORT_DIR)/src/lib/qt6/genode/api/lib/symbols/$@ lib/symbols/

content: LICENSE

LICENSE:
	cp $(PORT_DIR)/src/lib/qt6/LICENSE.LGPL3 $@
