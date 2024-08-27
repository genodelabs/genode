TARGET = qt5_declarative.qmake_target

QT5_PORT_LIBS = libQt5Core libQt5Gui libQt5Network libQt5Sql libQt5Test libQt5Widgets

LIBS = qt5_qmake ldso_so_support libc libm mesa stdcxx

INSTALL_LIBS = lib/libQt5Qml.lib.so \
               lib/libQt5QmlModels.lib.so \
               lib/libQt5QmlWorkerScript.lib.so \
               lib/libQt5Quick.lib.so \
               lib/libQt5QuickParticles.lib.so \
               lib/libQt5QuickShapes.lib.so \
               lib/libQt5QuickTest.lib.so \
               lib/libQt5QuickWidgets.lib.so \
               qml/Qt/labs/animation/liblabsanimationplugin.lib.so \
               qml/Qt/labs/folderlistmodel/libqmlfolderlistmodelplugin.lib.so \
               qml/Qt/labs/qmlmodels/liblabsmodelsplugin.lib.so \
               qml/Qt/labs/settings/libqmlsettingsplugin.lib.so \
               qml/Qt/labs/wavefrontmesh/libqmlwavefrontmeshplugin.lib.so \
               qml/QtQml/Models.2/libmodelsplugin.lib.so \
               qml/QtQml/StateMachine/libqtqmlstatemachine.lib.so \
               qml/QtQml/WorkerScript.2/libworkerscriptplugin.lib.so \
               qml/QtQml/libqmlplugin.lib.so \
               qml/QtQuick.2/libqtquick2plugin.lib.so \
               qml/QtQuick/Layouts/libqquicklayoutsplugin.lib.so \
               qml/QtQuick/LocalStorage/libqmllocalstorageplugin.lib.so \
               qml/QtQuick/Particles.2/libparticlesplugin.lib.so \
               qml/QtQuick/Shapes/libqmlshapesplugin.lib.so \
               qml/QtQuick/Window.2/libwindowplugin.lib.so \
               qml/QtTest/libqmltestplugin.lib.so

BUILD_ARTIFACTS = $(notdir $(INSTALL_LIBS)) \
                  qt5_declarative_qml.tar

build: qmake_prepared.tag qt5_so_files

	@#
	@# run qmake
	@#

	$(VERBOSE)source env.sh && $(QMAKE) \
		-qtconf build_dependencies/mkspecs/$(QT_PLATFORM)/qt.conf \
		$(QT_DIR)/qtdeclarative/qtdeclarative.pro \
		-- \
		-no-feature-qml-devtools \
		-no-feature-qml-jit \
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

	$(VERBOSE)tar chf $(PWD)/bin/qt5_declarative_qml.tar $(TAR_OPT) --exclude='*.lib.so' --transform='s/\.stripped//' -C install qt/qml

.PHONY: build

QT5_TARGET_DEPS = build
