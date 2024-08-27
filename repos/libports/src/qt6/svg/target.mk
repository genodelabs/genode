TARGET = qt6_svg.cmake_target

QT6_PORT_LIBS = libQt6Core libQt6Gui libQt6Widgets

LIBS = qt6_cmake ldso_so_support libc libm egl mesa qt6_component stdcxx

INSTALL_LIBS = lib/libQt6Svg.lib.so

BUILD_ARTIFACTS = $(notdir $(INSTALL_LIBS))

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
		$(QT_DIR)/qtsvg \
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

.PHONY: build

QT6_TARGET_DEPS = build
