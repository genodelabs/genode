TARGET = qt5_svg.qmake_target

QT5_PORT_LIBS = libQt5Core libQt5Gui libQt5Widgets

LIBS = qt5_qmake libc libm mesa stdcxx

INSTALL_LIBS = lib/libQt5Svg.lib.so \
               plugins/imageformats/libqsvg.lib.so

BUILD_ARTIFACTS = $(notdir $(INSTALL_LIBS)) \
                  qt5_libqsvg.tar

build: qmake_prepared.tag qt5_so_files

	@#
	@# run qmake
	@#

	$(VERBOSE)source env.sh && $(QMAKE) \
		-qtconf build_dependencies/mkspecs/$(QT_PLATFORM)/qt.conf \
		$(QT_DIR)/qtsvg/qtsvg.pro \
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

	$(VERBOSE)tar chf $(PWD)/bin/qt5_libqsvg.tar $(TAR_OPT) --transform='s/\.stripped//' -C install qt/plugins/imageformats/libqsvg.lib.so.stripped

.PHONY: build

QT5_TARGET_DEPS = build
