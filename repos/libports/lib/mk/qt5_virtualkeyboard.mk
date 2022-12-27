include $(call select_from_repositories,lib/import/import-qt5_qmake.mk)

QT5_PORT_LIBS  = libQt5Core libQt5Gui libQt5Network libQt5Widgets
QT5_PORT_LIBS += libQt5Qml libQt5QmlModels libQt5Quick
QT5_PORT_LIBS += libQt5Svg

LIBS = libc libm mesa stdcxx $(QT5_PORT_LIBS)

built.tag: qmake_prepared.tag

	@#
	@# run qmake
	@#

	$(VERBOSE)source env.sh && $(QMAKE) \
		-qtconf qmake_root/mkspecs/$(QMAKE_PLATFORM)/qt.conf \
		$(QT_DIR)/qtvirtualkeyboard/qtvirtualkeyboard.pro \
		$(QT5_OUTPUT_FILTER)

	@#
	@# build
	@#

	$(VERBOSE)source env.sh && $(MAKE) sub-src $(QT5_OUTPUT_FILTER)

	@#
	@# install into local 'install' directory
	@#

	$(VERBOSE)$(MAKE) INSTALL_ROOT=$(CURDIR)/install sub-src-install_subtargets $(QT5_OUTPUT_FILTER)

	$(VERBOSE)ln -sf .$(CURDIR)/qmake_root install/qt

	@#
	@# create stripped versions
	@#

	$(VERBOSE)cd $(CURDIR)/install/qt/lib && \
		$(STRIP) libQt5VirtualKeyboard.lib.so -o libQt5VirtualKeyboard.lib.so.stripped

	$(VERBOSE)cd $(CURDIR)/install/qt/plugins/platforminputcontexts && \
		$(STRIP) libqtvirtualkeyboardplugin.lib.so -o libqtvirtualkeyboardplugin.lib.so.stripped

	$(VERBOSE)cd $(CURDIR)/install/qt/qml/QtQuick/VirtualKeyboard && \
		$(STRIP) libqtquickvirtualkeyboardplugin.lib.so -o libqtquickvirtualkeyboardplugin.lib.so.stripped

	$(VERBOSE)cd $(CURDIR)/install/qt/qml/QtQuick/VirtualKeyboard/Settings && \
		$(STRIP) libqtquickvirtualkeyboardsettingsplugin.lib.so -o libqtquickvirtualkeyboardsettingsplugin.lib.so.stripped

	$(VERBOSE)cd $(CURDIR)/install/qt/qml/QtQuick/VirtualKeyboard/Styles && \
		$(STRIP) libqtquickvirtualkeyboardstylesplugin.lib.so -o libqtquickvirtualkeyboardstylesplugin.lib.so.stripped

	@#
	@# create symlinks in 'bin' directory
	@#

	$(VERBOSE)ln -sf $(CURDIR)/install/qt/lib/libQt5VirtualKeyboard.lib.so.stripped $(PWD)/bin/libQt5VirtualKeyboard.lib.so

	$(VERBOSE)ln -sf $(CURDIR)/install/qt/plugins/platforminputcontexts/libqtvirtualkeyboardplugin.lib.so.stripped $(PWD)/bin/libqtvirtualkeyboardplugin.lib.so

	$(VERBOSE)ln -sf $(CURDIR)/install/qt/qml/QtQuick/VirtualKeyboard/libqtquickvirtualkeyboardplugin.lib.so.stripped $(PWD)/bin/libqtquickvirtualkeyboardplugin.lib.so
	$(VERBOSE)ln -sf $(CURDIR)/install/qt/qml/QtQuick/VirtualKeyboard/Settings/libqtquickvirtualkeyboardsettingsplugin.lib.so.stripped $(PWD)/bin/libqtquickvirtualkeyboardsettingsplugin.lib.so
	$(VERBOSE)ln -sf $(CURDIR)/install/qt/qml/QtQuick/VirtualKeyboard/Styles/libqtquickvirtualkeyboardstylesplugin.lib.so.stripped $(PWD)/bin/libqtquickvirtualkeyboardstylesplugin.lib.so

	@#
	@# create symlinks in 'debug' directory
	@#

	$(VERBOSE)ln -sf $(CURDIR)/install/qt/lib/libQt5VirtualKeyboard.lib.so $(PWD)/debug/

	$(VERBOSE)ln -sf $(CURDIR)/install/qt/plugins/platforminputcontexts/libqtvirtualkeyboardplugin.lib.so $(PWD)/debug/

	$(VERBOSE)ln -sf $(CURDIR)/install/qt/qml/QtQuick/VirtualKeyboard/libqtquickvirtualkeyboardplugin.lib.so $(PWD)/debug/
	$(VERBOSE)ln -sf $(CURDIR)/install/qt/qml/QtQuick/VirtualKeyboard/Settings/libqtquickvirtualkeyboardsettingsplugin.lib.so $(PWD)/debug/
	$(VERBOSE)ln -sf $(CURDIR)/install/qt/qml/QtQuick/VirtualKeyboard/Styles/libqtquickvirtualkeyboardstylesplugin.lib.so $(PWD)/debug/

	@#
	@# create tar archives
	@#

	$(VERBOSE)tar chf $(PWD)/bin/qt5_libqtvirtualkeyboardplugin.tar --transform='s/\.stripped//' -C install qt/plugins/platforminputcontexts/libqtvirtualkeyboardplugin.lib.so.stripped
	$(VERBOSE)tar chf $(PWD)/bin/qt5_virtualkeyboard_qml.tar --exclude='*.lib.so' --transform='s/\.stripped//' -C install qt/qml

	@#
	@# mark as done
	@#

	$(VERBOSE)touch $@


ifeq ($(called_from_lib_mk),yes)
all: built.tag
endif
