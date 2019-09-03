MIRROR_FROM_REP_DIR := lib/mk/qt5_qtvirtualkeyboardplugin.mk \
                       lib/mk/qt5_qtvirtualkeyboardplugin_generated.inc \
                       lib/mk/qt5_qtquickvirtualkeyboardplugin.mk \
                       lib/mk/qt5_qtquickvirtualkeyboardplugin_generated.inc \
                       lib/mk/qt5_qtquickvirtualkeyboardsettingsplugin.mk \
                       lib/mk/qt5_qtquickvirtualkeyboardsettingsplugin_generated.inc \
                       lib/mk/qt5_qtquickvirtualkeyboardstylesplugin.mk \
                       lib/mk/qt5_qtquickvirtualkeyboardstylesplugin_generated.inc \
                       lib/mk/qt5.inc \

content: $(MIRROR_FROM_REP_DIR) src/lib/qt5_qtvirtualkeyboardplugin/target.mk \
                                src/lib/qt5_qtquickvirtualkeyboardplugin/target.mk \
                                src/lib/qt5_qtquickvirtualkeyboardsettingsplugin/target.mk \
                                src/lib/qt5_qtquickvirtualkeyboardstylesplugin/target.mk

$(MIRROR_FROM_REP_DIR):
	$(mirror_from_rep_dir)

src/lib/qt5_qtvirtualkeyboardplugin/target.mk:
	mkdir -p $(dir $@)
	echo "LIBS = qt5_qtvirtualkeyboardplugin" > $@

src/lib/qt5_qtquickvirtualkeyboardplugin/target.mk:
	mkdir -p $(dir $@)
	echo "LIBS = qt5_qtquickvirtualkeyboardplugin" > $@

src/lib/qt5_qtquickvirtualkeyboardsettingsplugin/target.mk:
	mkdir -p $(dir $@)
	echo "LIBS = qt5_qtquickvirtualkeyboardsettingsplugin" > $@

src/lib/qt5_qtquickvirtualkeyboardstylesplugin/target.mk:
	mkdir -p $(dir $@)
	echo "LIBS = qt5_qtquickvirtualkeyboardstylesplugin" > $@

PORT_DIR := $(call port_dir,$(REP_DIR)/ports/qt5)

MIRROR_FROM_PORT_DIR := src/lib/qt5/qt5/qtvirtualkeyboard/src/import \
                        src/lib/qt5/qt5/qtvirtualkeyboard/src/plugin \
                        src/lib/qt5/qt5/qtvirtualkeyboard/src/settings \
                        src/lib/qt5/qt5/qtvirtualkeyboard/src/styles

content: $(MIRROR_FROM_PORT_DIR)

$(MIRROR_FROM_PORT_DIR):
	mkdir -p $(dir $@)
	cp -r $(PORT_DIR)/$@ $(dir $@)

content: LICENSE

LICENSE:
	cp $(PORT_DIR)/src/lib/qt5/qt5/LICENSE.LGPLv3 $@
