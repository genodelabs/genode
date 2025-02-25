TARGET = qt6_declarative.cmake_target

ifeq ($(CONTRIB_DIR),)
QT6_DECLARATIVE_DIR       = $(call select_from_repositories,src/lib/qt6_declarative)
else
QT6_DECLARATIVE_PORT_DIR := $(call select_from_ports,qt6_declarative)
QT6_DECLARATIVE_DIR       = $(QT6_DECLARATIVE_PORT_DIR)/src/lib/qt6_declarative
endif

QT6_PORT_LIBS = libQt6Core libQt6Gui libQt6OpenGL libQt6Network libQt6Sql libQt6Test libQt6Widgets

LIBS = qt6_cmake ldso_so_support libc libm mesa egl qt6_component stdcxx

INSTALL_LIBS = lib/libQt6LabsAnimation.lib.so \
               lib/libQt6LabsFolderListModel.lib.so \
               lib/libQt6LabsQmlModels.lib.so \
               lib/libQt6LabsSettings.lib.so \
               lib/libQt6LabsSharedImage.lib.so \
               lib/libQt6LabsWavefrontMesh.lib.so \
               lib/libQt6Qml.lib.so \
               lib/libQt6QmlCompiler.lib.so \
               lib/libQt6QmlCore.lib.so \
               lib/libQt6QmlLocalStorage.lib.so \
               lib/libQt6QmlModels.lib.so \
               lib/libQt6QmlWorkerScript.lib.so \
               lib/libQt6QmlXmlListModel.lib.so \
               lib/libQt6Quick.lib.so \
               lib/libQt6QuickControls2.lib.so \
               lib/libQt6QuickControls2Impl.lib.so \
               lib/libQt6QuickDialogs2.lib.so \
               lib/libQt6QuickDialogs2QuickImpl.lib.so \
               lib/libQt6QuickDialogs2Utils.lib.so \
               lib/libQt6QuickEffects.lib.so \
               lib/libQt6QuickLayouts.lib.so \
               lib/libQt6QuickParticles.lib.so \
               lib/libQt6QuickShapes.lib.so \
               lib/libQt6QuickTemplates2.lib.so \
               lib/libQt6QuickTest.lib.so \
               lib/libQt6QuickWidgets.lib.so \
               qml/Qt/labs/animation/liblabsanimationplugin.lib.so \
               qml/Qt/labs/folderlistmodel/libqmlfolderlistmodelplugin.lib.so \
               qml/Qt/labs/platform/libqtlabsplatformplugin.lib.so \
               qml/Qt/labs/qmlmodels/liblabsmodelsplugin.lib.so \
               qml/Qt/labs/settings/libqmlsettingsplugin.lib.so \
               qml/Qt/labs/sharedimage/libsharedimageplugin.lib.so \
               qml/Qt/labs/wavefrontmesh/libqmlwavefrontmeshplugin.lib.so \
               qml/QtCore/libqtqmlcoreplugin.lib.so \
               qml/QtQml/Base/libqmlplugin.lib.so \
               qml/QtQml/Models/libmodelsplugin.lib.so \
               qml/QtQml/WorkerScript/libworkerscriptplugin.lib.so \
               qml/QtQml/XmlListModel/libqmlxmllistmodelplugin.lib.so \
               qml/QtQml/libqmlmetaplugin.lib.so \
               qml/QtQuick/Controls/Basic/libqtquickcontrols2basicstyleplugin.lib.so \
               qml/QtQuick/Controls/Basic/impl/libqtquickcontrols2basicstyleimplplugin.lib.so \
               qml/QtQuick/Controls/Fusion/libqtquickcontrols2fusionstyleplugin.lib.so \
               qml/QtQuick/Controls/Fusion/impl/libqtquickcontrols2fusionstyleimplplugin.lib.so \
               qml/QtQuick/Controls/Imagine/libqtquickcontrols2imaginestyleplugin.lib.so \
               qml/QtQuick/Controls/Imagine/impl/libqtquickcontrols2imaginestyleimplplugin.lib.so \
               qml/QtQuick/Controls/Material/libqtquickcontrols2materialstyleplugin.lib.so \
               qml/QtQuick/Controls/Material/impl/libqtquickcontrols2materialstyleimplplugin.lib.so \
               qml/QtQuick/Controls/Universal/libqtquickcontrols2universalstyleplugin.lib.so \
               qml/QtQuick/Controls/Universal/impl/libqtquickcontrols2universalstyleimplplugin.lib.so \
               qml/QtQuick/Controls/libqtquickcontrols2plugin.lib.so \
               qml/QtQuick/Controls/impl/libqtquickcontrols2implplugin.lib.so \
               qml/QtQuick/Dialogs/libqtquickdialogsplugin.lib.so \
               qml/QtQuick/Dialogs/quickimpl/libqtquickdialogs2quickimplplugin.lib.so \
               qml/QtQuick/Effects/libeffectsplugin.lib.so \
               qml/QtQuick/Layouts/libqquicklayoutsplugin.lib.so \
               qml/QtQuick/LocalStorage/libqmllocalstorageplugin.lib.so \
               qml/QtQuick/NativeStyle/libqtquickcontrols2nativestyleplugin.lib.so \
               qml/QtQuick/Particles/libparticlesplugin.lib.so \
               qml/QtQuick/Shapes/libqmlshapesplugin.lib.so \
               qml/QtQuick/Templates/libqtquicktemplates2plugin.lib.so \
               qml/QtQuick/Window/libquickwindowplugin.lib.so \
               qml/QtQuick/tooling/libquicktoolingplugin.lib.so \
               qml/QtQuick/libqtquick2plugin.lib.so \
               qml/QtTest/libquicktestplugin.lib.so

BUILD_ARTIFACTS = $(notdir $(INSTALL_LIBS)) \
                  qt6_declarative_qml.tar

build: cmake_prepared.tag qt6_so_files

	@#
	@# run cmake
	@#

	$(VERBOSE)cmake \
		-G "Unix Makefiles" \
		-DCMAKE_PREFIX_PATH="$(CURDIR)/build_dependencies" \
		-DCMAKE_MODULE_PATH="$(CURDIR)/build_dependencies/lib/cmake/Modules" \
		-DCMAKE_SYSTEM_NAME="Genode" \
		-DCMAKE_AR="$(AR)" \
		-DCMAKE_C_COMPILER="$(CC)" \
		-DCMAKE_C_FLAGS="$(GENODE_CMAKE_CFLAGS)" \
		-DCMAKE_CXX_COMPILER="$(CXX)" \
		-DCMAKE_CXX_FLAGS="$(GENODE_CMAKE_CFLAGS)" \
		-DCMAKE_EXE_LINKER_FLAGS="$(GENODE_CMAKE_LFLAGS_APP)" \
		-DCMAKE_SHARED_LINKER_FLAGS="$(GENODE_CMAKE_LFLAGS_SHLIB)" \
		-DCMAKE_MODULE_LINKER_FLAGS="$(GENODE_CMAKE_LFLAGS_SHLIB)" \
		-DQT_QMAKE_TARGET_MKSPEC=$(QT_PLATFORM) \
		-DCMAKE_INSTALL_PREFIX=/qt \
		$(QT6_DECLARATIVE_DIR) \
		$(QT6_OUTPUT_FILTER)

	@#
	@# build
	@#

	$(VERBOSE)$(MAKE) VERBOSE=$(MAKE_VERBOSE)

	@#
	@# install into local 'install' directory
	@#

	$(VERBOSE)$(MAKE) VERBOSE=$(MAKE_VERBOSE) DESTDIR=install install

	@#
	@# remove shared library existence checks since many libs are not
	@# present and not needed at build time
	@#

	$(VERBOSE)find $(CURDIR)/install/qt/lib/cmake -name "*.cmake" \
	          -exec sed -i "/list(APPEND _IMPORT_CHECK_TARGETS /d" {} \;

	@#
	@# strip libs and create symlinks in 'bin' and 'debug' directories
	@#

	$(VERBOSE)for LIB in $(INSTALL_LIBS); do \
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

	$(VERBOSE)tar chf $(PWD)/bin/qt6_declarative_qml.tar $(TAR_OPT) --exclude='*.lib.so' --transform='s/\.stripped//' -C install qt/qml

.PHONY: build

QT6_TARGET_DEPS = build
