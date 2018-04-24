MIRROR_FROM_REP_DIR := lib/mk/qt5_angle.mk \
                       lib/mk/qt5_angle_generated.inc \
                       lib/mk/qt5_jscore.mk \
                       lib/mk/qt5_jscore_generated.inc \
                       lib/mk/qt5_webcore.mk \
                       lib/mk/qt5_webcore_generated.inc \
                       lib/mk/qt5_webkit.mk \
                       lib/mk/qt5_webkit_generated.inc \
                       lib/mk/qt5_webkitwidgets.mk \
                       lib/mk/qt5_webkitwidgets_generated.inc \
                       lib/mk/qt5_wtf.mk \
                       lib/mk/qt5_wtf_generated.inc \
                       lib/mk/qt5.inc

content: $(MIRROR_FROM_REP_DIR) \
         src/lib/qt5_angle/target.mk \
         src/lib/qt5_jscore/target.mk \
         src/lib/qt5_webcore/target.mk \
         src/lib/qt5_webkit/target.mk \
         src/lib/qt5_webkitwidgets/target.mk \
         src/lib/qt5_wtf/target.mk

$(MIRROR_FROM_REP_DIR):
	$(mirror_from_rep_dir)

src/lib/qt5_angle/target.mk:
	mkdir -p $(dir $@)
	echo "LIBS = qt5_angle" > $@

src/lib/qt5_jscore/target.mk:
	mkdir -p $(dir $@)
	echo "LIBS = qt5_jscore" > $@

src/lib/qt5_webcore/target.mk:
	mkdir -p $(dir $@)
	echo "LIBS = qt5_webcore" > $@

src/lib/qt5_webkit/target.mk:
	mkdir -p $(dir $@)
	echo "LIBS = qt5_webkit" > $@

src/lib/qt5_webkitwidgets/target.mk:
	mkdir -p $(dir $@)
	echo "LIBS = qt5_webkitwidgets" > $@

src/lib/qt5_wtf/target.mk:
	mkdir -p $(dir $@)
	echo "LIBS = qt5_wtf" > $@

PORT_DIR := $(call port_dir,$(REP_DIR)/ports/qt5)

MIRROR_FROM_PORT_DIR := src/lib/qt5/qt5/qtbase/src/3rdparty/sqlite \
                        src/lib/qt5/qt5/qtwebkit/Source/ThirdParty/ANGLE \
                        src/lib/qt5/qt5/qtwebkit/Source/JavaScriptCore \
                        src/lib/qt5/qt5/qtwebkit/Source/WebCore \
                        src/lib/qt5/qt5/qtwebkit/Source/WebKit \
                        src/lib/qt5/qt5/qtwebkit/Source/WTF \
                        src/lib/qt5/qtwebkit/Source/ThirdParty/ANGLE/generated \
                        src/lib/qt5/qtwebkit/Source/JavaScriptCore/generated \
                        src/lib/qt5/qtwebkit/Source/WebCore/generated

content: $(MIRROR_FROM_PORT_DIR)

$(MIRROR_FROM_PORT_DIR):
	mkdir -p $(dir $@)
	cp -r $(PORT_DIR)/$@ $(dir $@)

content: LICENSE

LICENSE:
	cp $(PORT_DIR)/src/lib/qt5/qt5/LICENSE.LGPLv3 $@
