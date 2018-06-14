content: qt5_dejavusans.tar

PORT_DIR := $(call port_dir,$(REP_DIR)/ports/qt5)

qt/lib/fonts/DejaVuSans.ttf:
	mkdir -p $(dir $@)
	cp $(PORT_DIR)/src/lib/qt5/qt5/qtquickcontrols/examples/quickcontrols/extras/dashboard/fonts/$(notdir $@) $@

qt5_dejavusans.tar: qt/lib/fonts/DejaVuSans.ttf
	tar --owner=0 --group=0 --numeric-owner --mode='go=' --mtime='1970-01-01' -cf $@ qt
	rm -rf qt
