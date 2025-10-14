content: qt6_dejavusans.tar LICENSE

PORT_DIR := $(call port_dir,$(REP_DIR)/ports/qt6_base)

include $(GENODE_DIR)/repos/base/recipes/content.inc

qt/lib/fonts/DejaVuSans.ttf:
	mkdir -p $(dir $@)
	cp $(PORT_DIR)/src/lib/qt6_base/src/3rdparty/wasm/$(notdir $@) $@

qt6_dejavusans.tar: qt/lib/fonts/DejaVuSans.ttf
	$(TAR) -cf $@ qt
	rm -rf qt

LICENSE:
	cp $(PORT_DIR)/src/lib/qt6_base/src/3rdparty/wasm/DEJAVU-LICENSE $@
