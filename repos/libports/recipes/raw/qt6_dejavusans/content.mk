content: qt6_dejavusans.tar LICENSE

PORT_DIR := $(call port_dir,$(REP_DIR)/ports/qt6)

qt/lib/fonts/DejaVuSans.ttf:
	mkdir -p $(dir $@)
	cp $(PORT_DIR)/src/lib/qt6/qtbase/src/3rdparty/wasm/$(notdir $@) $@

qt6_dejavusans.tar: qt/lib/fonts/DejaVuSans.ttf
	tar --owner=0 --group=0 --numeric-owner --mode='go=' --mtime='1970-01-01 00:00+00' -cf $@ qt
	rm -rf qt

LICENSE:
	cp $(PORT_DIR)/src/lib/qt6/qtbase/src/3rdparty/wasm/DEJAVU-LICENSE $@
