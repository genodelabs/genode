include $(call select_from_repositories,lib/import/import-qt5_qmake.mk)

LIBS = base libc libm stdcxx qt5_component egl mesa qoost

INSTALL_LIBS = lib/libQt5Core.lib.so \
               lib/libQt5Gui.lib.so \
               lib/libQt5Network.lib.so \
               lib/libQt5PrintSupport.lib.so \
               lib/libQt5Sql.lib.so \
               lib/libQt5Test.lib.so \
               lib/libQt5Widgets.lib.so \
               lib/libQt5Xml.lib.so \
               plugins/platforms/libqgenode.lib.so \
               plugins/imageformats/libqjpeg.lib.so \
               plugins/sqldrivers/libqsqlite.lib.so

BUILD_ARTIFACTS = $(notdir $(INSTALL_LIBS)) \
                  qt5_libqgenode.tar \
                  qt5_libqjpeg.tar \
                  qt5_libqsqlite.tar

build: qmake_prepared.tag qmake_root/lib/ld.lib.so

	@#
	@# run qmake
	@#

	$(VERBOSE)source env.sh && $(QMAKE) \
		-qtconf qmake_root/mkspecs/$(QMAKE_PLATFORM)/qt.conf \
		$(QT_DIR)/qtbase/qtbase.pro \
		-- \
		-prefix /qt \
		-xplatform $(QMAKE_PLATFORM) \
		-qpa genode \
		-opensource \
		-confirm-license \
		-no-pch \
		-release \
		-force-debug-info \
		-no-strip \
		-opengl desktop \
		-no-feature-dbus \
		-no-feature-networkinterface \
		-no-feature-process \
		-no-feature-relocatable \
		-no-feature-vulkan \
		$(QT5_OUTPUT_FILTER)

	@#
	@# build
	@#

	$(VERBOSE)source env.sh && $(MAKE) sub-src $(QT5_OUTPUT_FILTER)

	@#
	@# install into local 'install' directory
	@#

	$(VERBOSE)$(MAKE) INSTALL_ROOT=$(CURDIR)/install sub-src-install_subtargets $(QT5_OUTPUT_FILTER)

	$(VERBOSE) ln -sf .$(CURDIR)/qmake_root install/qt

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

	$(VERBOSE)tar chf $(PWD)/bin/qt5_libqgenode.tar $(TAR_OPT) --transform='s/\.stripped//' -C install qt/plugins/platforms/libqgenode.lib.so.stripped
	$(VERBOSE)tar chf $(PWD)/bin/qt5_libqjpeg.tar   $(TAR_OPT) --transform='s/\.stripped//' -C install qt/plugins/imageformats/libqjpeg.lib.so.stripped
	$(VERBOSE)tar chf $(PWD)/bin/qt5_libqsqlite.tar $(TAR_OPT) --transform='s/\.stripped//' -C install qt/plugins/sqldrivers/libqsqlite.lib.so.stripped

.PHONY: build

ifeq ($(called_from_lib_mk),yes)
all: build
endif
