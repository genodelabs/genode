TARGET = qt5_quickcontrols2.qmake_target

QT5_PORT_LIBS += libQt5Core libQt5Gui libQt5Network libQt5Widgets
QT5_PORT_LIBS += libQt5Qml libQt5QmlModels libQt5Quick

LIBS = qt5_qmake libc libm mesa stdcxx

INSTALL_LIBS = lib/libQt5QuickControls2.lib.so \
               lib/libQt5QuickTemplates2.lib.so \
               qml/Qt/labs/calendar/libqtlabscalendarplugin.lib.so \
               qml/Qt/labs/platform/libqtlabsplatformplugin.lib.so \
               qml/QtQuick/Controls.2/libqtquickcontrols2plugin.lib.so \
               qml/QtQuick/Controls.2/Fusion/libqtquickcontrols2fusionstyleplugin.lib.so \
               qml/QtQuick/Controls.2/Imagine/libqtquickcontrols2imaginestyleplugin.lib.so \
               qml/QtQuick/Controls.2/Material/libqtquickcontrols2materialstyleplugin.lib.so \
               qml/QtQuick/Controls.2/Universal/libqtquickcontrols2universalstyleplugin.lib.so \
               qml/QtQuick/Templates.2/libqtquicktemplates2plugin.lib.so

BUILD_ARTIFACTS = $(notdir $(INSTALL_LIBS)) \
                  qt5_quickcontrols2_qml.tar

build: qmake_prepared.tag qt5_so_files

	@#
	@# run qmake
	@#

	$(VERBOSE)source env.sh && $(QMAKE) \
		-qtconf build_dependencies/mkspecs/$(QT_PLATFORM)/qt.conf \
		$(QT_DIR)/qtquickcontrols2/qtquickcontrols2.pro \
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

	$(VERBOSE)tar chf $(PWD)/bin/qt5_quickcontrols2_qml.tar $(TAR_OPT) --exclude='*.lib.so' --transform='s/\.stripped//' -C install qt/qml

.PHONY: build

QT5_TARGET_DEPS = build
