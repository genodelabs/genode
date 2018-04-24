MIRROR_FROM_REP_DIR := lib/mk/qt5_ui_tools.mk \
                       lib/mk/qt5_ui_tools_generated.inc \
                       lib/mk/qt5.inc \

content: $(MIRROR_FROM_REP_DIR) src/lib/qt5_ui_tools/target.mk

$(MIRROR_FROM_REP_DIR):
	$(mirror_from_rep_dir)

src/lib/qt5_ui_tools/target.mk:
	mkdir -p $(dir $@)
	echo "LIBS = qt5_ui_tools" > $@

PORT_DIR := $(call port_dir,$(REP_DIR)/ports/qt5)

MIRROR_FROM_PORT_DIR := src/lib/qt5/qt5/qttools/src/designer/src/lib/uilib \
                        src/lib/qt5/qt5/qttools/src/designer/src/uitools

content: $(MIRROR_FROM_PORT_DIR)

$(MIRROR_FROM_PORT_DIR):
	mkdir -p $(dir $@)
	cp -r $(PORT_DIR)/$@ $(dir $@)

content: LICENSE

LICENSE:
	cp $(PORT_DIR)/src/lib/qt5/qt5/LICENSE.LGPLv3 $@
