MIRROR_FROM_REP_DIR := lib/mk/qt5_qpa_nitpicker.mk \
                       lib/mk/qt5.inc \
                       src/lib/qt5/qtbase/src/plugins/platforms/nitpicker

content: $(MIRROR_FROM_REP_DIR) src/lib/qt5_qpa_nitpicker/target.mk

$(MIRROR_FROM_REP_DIR):
	$(mirror_from_rep_dir)

src/lib/qt5_qpa_nitpicker/target.mk:
	mkdir -p $(dir $@)
	echo "LIBS = qt5_qpa_nitpicker" > $@

PORT_DIR := $(call port_dir,$(REP_DIR)/ports/qt5)

MIRROR_FROM_PORT_DIR := src/lib/qt5/qt5/qtbase/src/platformsupport/eglconvenience/qeglconvenience.cpp \
                        src/lib/qt5/qt5/qtbase/src/platformsupport/eventdispatchers/qgenericunixeventdispatcher.cpp \
                        src/lib/qt5/qt5/qtbase/src/platformsupport/eventdispatchers/qunixeventdispatcher.cpp \
                        src/lib/qt5/qt5/qtbase/src/platformsupport/fontdatabases/freetype/qfontengine_ft.cpp \
                        src/lib/qt5/qt5/qtbase/src/platformsupport/fontdatabases/freetype/qfreetypefontdatabase.cpp



content: $(MIRROR_FROM_PORT_DIR)

$(MIRROR_FROM_PORT_DIR):
	mkdir -p $(dir $@)
	cp -r $(PORT_DIR)/$@ $(dir $@)

MIRROR_FROM_OS := include/pointer/shape_report.h

content: $(MIRROR_FROM_OS)

$(MIRROR_FROM_OS):
	mkdir -p $(dir $@)
	cp -r $(GENODE_DIR)/repos/os/$@ $(dir $@)

content: LICENSE

LICENSE:
	cp $(GENODE_DIR)/LICENSE $@
