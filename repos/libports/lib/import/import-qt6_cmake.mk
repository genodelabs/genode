#
# The following externally defined variables are evaluated:
#
# CMAKE_LISTS_DIR:       path to the CMakeLists.txt file
# CMAKE_TARGET_BINARIES  binaries to be stripped and linked into 'bin' and 'debug' directories
# QT6_PORT_LIBS:         Qt6 libraries used from port (for example libQt6Core)
# QT6_COMPONENT_LIB_SO:  if defined empty, disables linking with qt6_component.lib.so
# QT6_TARGET_DEPS:       default is 'build_with_cmake'
# QT6_EXTRA_TARGET_DEPS: additional target dependencies
#

include $(call select_from_repositories,lib/import/import-qt6.inc)

#
# flags to be passed to CMake
#

GENODE_CMAKE_CFLAGS += \
	-D__FreeBSD__=12 \
	-D__GENODE__ \
	-ffunction-sections \
	-fno-strict-aliasing \
	$(CC_OPT_NOSTDINC) \
	$(CC_MARCH) \
	$(CC_OPT_PIC) \
	$(filter-out -I.,$(INCLUDES)) \
	-I$(CURDIR)/build_dependencies/mkspecs/$(QT_PLATFORM)

GENODE_CMAKE_LFLAGS_APP += \
	$(addprefix $(LD_OPT_PREFIX),$(LD_MARCH)) \
	$(addprefix $(LD_OPT_PREFIX),$(LD_OPT_GC_SECTIONS)) \
	$(addprefix $(LD_OPT_PREFIX),$(LD_OPT_ALIGN_SANE)) \
	$(addprefix $(LD_OPT_PREFIX),--dynamic-list=$(BASE_DIR)/src/ld/genode_dyn.dl) \
	$(LD_OPT_NOSTDLIB) \
	-Wl,-Ttext=0x01000000 \
	$(CC_MARCH) \
	-Wl,--dynamic-linker=$(DYNAMIC_LINKER).lib.so \
	-Wl,--eh-frame-hdr \
	-Wl,-rpath-link=. \
	-Wl,-T -Wl,$(LD_SCRIPT_DYN) \
	-L$(CURDIR)/build_dependencies/lib \
	-Wl,--whole-archive \
	-Wl,--start-group \
	$(addprefix -l:,$(QT6_GENODE_LIBS_APP)) \
	$(shell $(CC) $(CC_MARCH) -print-libgcc-file-name) \
	-Wl,--end-group \
	-Wl,--no-whole-archive

GENODE_CMAKE_LFLAGS_SHLIB += \
	$(LD_OPT_NOSTDLIB) \
	-Wl,-shared \
	-Wl,--eh-frame-hdr \
	$(addprefix $(LD_OPT_PREFIX),$(LD_MARCH)) \
	$(addprefix $(LD_OPT_PREFIX),$(LD_OPT_GC_SECTIONS)) \
	$(addprefix $(LD_OPT_PREFIX),$(LD_OPT_ALIGN_SANE)) \
	-Wl,-T -Wl,$(LD_SCRIPT_SO) \
	$(addprefix $(LD_OPT_PREFIX),--entry=0x0) \
	-L$(CURDIR)/build_dependencies/lib \
	-Wl,--whole-archive \
	-Wl,--start-group \
	$(addprefix -l:,$(QT6_GENODE_LIBS_SHLIB)) \
	$(shell $(CC) $(CC_MARCH) -print-libgcc-file-name) \
	-l:ldso_so_support.lib.a \
	-Wl,--end-group \
	-Wl,--no-whole-archive

##
## prepare a directory named 'build_dependencies' where CMake can find needed files
##

build_dependencies/lib/cmake: build_dependencies/lib
	$(VERBOSE)ln -snf $(QT_API_DIR)/lib/cmake $@

build_dependencies/metatypes: build_dependencies
	$(VERBOSE)ln -snf $(QT_API_DIR)/metatypes $@

build_dependencies/mkspecs: build_dependencies
	$(VERBOSE)ln -snf $(QT_API_DIR)/mkspecs $@

cmake_prepared.tag: \
                    build_dependencies/include \
                    build_dependencies/lib/cmake \
                    build_dependencies/lib/libc.lib.so \
                    build_dependencies/lib/libm.lib.so \
                    build_dependencies/lib/egl.lib.so \
                    build_dependencies/lib/mesa.lib.so \
                    build_dependencies/lib/qt6_component.lib.so \
                    build_dependencies/lib/stdcxx.lib.so \
                    build_dependencies/lib/ldso_so_support.lib.a \
                    build_dependencies/metatypes \
                    build_dependencies/mkspecs
	$(VERBOSE)touch $@

.PHONY: build_with_cmake

# 'make' called by CMake uses '/bin/sh', which does not understand '-o pipefail'
unexport .SHELLFLAGS

build_with_cmake: cmake_prepared.tag qt6_so_files
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
	--no-warn-unused-cli \
	$(CMAKE_LISTS_DIR) \
	$(QT6_OUTPUT_FILTER)

	$(VERBOSE)$(MAKE) VERBOSE=$(MAKE_VERBOSE)

#
# Not every CMake project has an 'install' target, so execute
# this target only if a binary to be installed has "install/" in
# its path.
#
ifneq ($(findstring install/,$(CMAKE_TARGET_BINARIES)),)
	$(VERBOSE)$(MAKE) VERBOSE=$(MAKE_VERBOSE) DESTDIR=install install
endif

	$(VERBOSE)for cmake_target_binary in $(CMAKE_TARGET_BINARIES); do \
		$(OBJCOPY) --only-keep-debug  $${cmake_target_binary} $${cmake_target_binary}.debug; \
		$(STRIP) $${cmake_target_binary} -o $${cmake_target_binary}.stripped; \
		$(OBJCOPY) --add-gnu-debuglink=$${cmake_target_binary}.debug $${cmake_target_binary}.stripped; \
		ln -sf $(CURDIR)/$${cmake_target_binary}.stripped $(PWD)/bin/`basename $${cmake_target_binary}`; \
		ln -sf $(CURDIR)/$${cmake_target_binary}.stripped $(PWD)/debug/`basename $${cmake_target_binary}`; \
		ln -sf $(CURDIR)/$${cmake_target_binary}.debug $(PWD)/debug/; \
	done

BUILD_ARTIFACTS += $(notdir $(CMAKE_TARGET_BINARIES))

#
# build with CMake by default
#

QT6_TARGET_DEPS ?= build_with_cmake

TARGET ?= $(CMAKE_LISTS_DIR).cmake_target
.PHONY: $(TARGET)
$(TARGET): $(QT6_TARGET_DEPS) $(QT6_EXTRA_TARGET_DEPS)
