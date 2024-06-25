include $(call select_from_repositories,lib/import/import-qt5_qmake.mk)

QT5_PORT_LIBS += libQt5Core libQt5Gui libQt5Network libQt5Widgets
QT5_PORT_LIBS += libQt5Qml libQt5QmlModels libQt5Quick

LIBS = libc libm mesa stdcxx $(QT5_PORT_LIBS)

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

build: qmake_prepared.tag

	@#
	@# run qmake
	@#

	$(VERBOSE)source env.sh && $(QMAKE) \
		-qtconf qmake_root/mkspecs/$(QMAKE_PLATFORM)/qt.conf \
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

	$(VERBOSE)ln -sf .$(CURDIR)/qmake_root install/qt

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

ifeq ($(called_from_lib_mk),yes)
all: build
endif
