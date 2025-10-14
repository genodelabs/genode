content: qt5_dejavusans.tar

PORT_DIR := $(call port_dir,$(REP_DIR)/ports/qt5)

qt/lib/fonts/DejaVuSans.ttf:
	mkdir -p $(dir $@)
	cp $(PORT_DIR)/src/lib/qt5/qtquickcontrols/examples/quickcontrols/extras/dashboard/fonts/$(notdir $@) $@

include $(GENODE_DIR)/repos/base/recipes/content.inc

qt5_dejavusans.tar: qt/lib/fonts/DejaVuSans.ttf
	$(TAR) -cf $@ qt
	rm -rf qt
