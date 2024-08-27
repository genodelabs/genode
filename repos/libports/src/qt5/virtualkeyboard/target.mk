TARGET = qt5_virtualkeyboard.qmake_target

QT5_PORT_LIBS  = libQt5Core libQt5Gui libQt5Network libQt5Widgets
QT5_PORT_LIBS += libQt5Qml libQt5QmlModels libQt5Quick
QT5_PORT_LIBS += libQt5Svg

LIBS = qt5_qmake libc libm mesa stdcxx

INSTALL_LIBS = lib/libQt5VirtualKeyboard.lib.so \
               plugins/platforminputcontexts/libqtvirtualkeyboardplugin.lib.so \
               qml/QtQuick/VirtualKeyboard/libqtquickvirtualkeyboardplugin.lib.so \
               qml/QtQuick/VirtualKeyboard/Settings/libqtquickvirtualkeyboardsettingsplugin.lib.so \
               qml/QtQuick/VirtualKeyboard/Styles/libqtquickvirtualkeyboardstylesplugin.lib.so

BUILD_ARTIFACTS = $(notdir $(INSTALL_LIBS)) \
                  qt5_libqtvirtualkeyboardplugin.tar \
                  qt5_virtualkeyboard_qml.tar

build: qmake_prepared.tag qt5_so_files

	@#
	@# run qmake
	@#

	$(VERBOSE)source env.sh && $(QMAKE) \
		-qtconf build_dependencies/mkspecs/$(QT_PLATFORM)/qt.conf \
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

	$(VERBOSE)ln -sf .$(CURDIR)/build_dependencies install/qt

	@#
	@# strip libs and create symlinks in 'bin' and 'debug' directories
	@#

	for LIB in $(INSTALL_LIBS); do \
		cd $(CURDIR)/install/qt/$$(dirname $${LIB}) && \
			$(OBJCOPY) --only-keep-debug $$(basename $${LIB}) $$(basename $${LIB}).debug && \
			$(STRIP) $$(basename $${LIB}) -o $$(basename $${LIB}).stripped && \
			$(OBJCOPY) --add-gnu-debuglink=$$(basename $${LIB}).debug $$(basename $${LIB}).stripped; \
		ln -sf $(CURDIR)/install/qt/$${LIB}.stripped $(PWD)/bin/$$(basename $${LIB}); \
		ln -sf $(CURDIR)/install/qt/$${LIB}.stripped $(PWD)/debug/$$(basename $${LIB}); \
		ln -sf $(CURDIR)/install/qt/$${LIB}.debug $(PWD)/debug/; \
	done

	@#
	@# create tar archives
	@#

	$(VERBOSE)tar chf $(PWD)/bin/qt5_libqtvirtualkeyboardplugin.tar $(TAR_OPT) --transform='s/\.stripped//' -C install qt/plugins/platforminputcontexts/libqtvirtualkeyboardplugin.lib.so.stripped
	$(VERBOSE)tar chf $(PWD)/bin/qt5_virtualkeyboard_qml.tar $(TAR_OPT) --exclude='*.lib.so' --transform='s/\.stripped//' -C install qt/qml

.PHONY: build

QT5_TARGET_DEPS = build
