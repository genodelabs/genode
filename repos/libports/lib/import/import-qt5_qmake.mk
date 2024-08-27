#
# The following externally defined variables are evaluated:
#
# QMAKE_PROJECT_FILE:    path to the qmake project file (for applications with target.mk)
# QMAKE_TARGET_BINARIES  binaries to be stripped and linked into 'bin' and 'debug' directories
# QT5_PORT_LIBS:         Qt5 libraries used from port (for example libQt5Core)
# QT5_COMPONENT_LIB_SO:  if defined empty, disables linking with qt5_component.lib.so
# QT5_TARGET_DEPS:       default is 'build_with_qmake'
# QT5_EXTRA_TARGET_DEPS: additional target dependencies
#

include $(call select_from_repositories,lib/import/import-qt5.inc)

QMAKE = $(QT_TOOLS_DIR)/bin/qmake

#
# flags to be passed to qmake via env.sh and mkspecs/common/genode.conf
#

GENODE_QMAKE_CFLAGS += \
	-D__FreeBSD__=12 \
	-D__GENODE__ \
	-ffunction-sections \
	-fno-strict-aliasing \
	$(CC_OPT_NOSTDINC) \
	$(CC_MARCH) \
	$(CC_OPT_PIC) \
	$(filter-out -I.,$(INCLUDES)) \
	-I$(CURDIR)/build_dependencies/include/QtCore/spec/$(QT_PLATFORM)

GENODE_QMAKE_LFLAGS_APP += \
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
	$(addprefix -l:,$(QT5_GENODE_LIBS_APP)) \
	-Wl,--end-group \
	-Wl,--no-whole-archive

GENODE_QMAKE_LFLAGS_SHLIB += \
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
	$(addprefix -l:,$(QT5_GENODE_LIBS_SHLIB)) \
	-l:ldso_so_support.lib.a \
	-Wl,--end-group \
	-Wl,--no-whole-archive

#
# libgcc must appear on the command line after all other libs
# (including those added by qmake) and using the QMAKE_LIBS
# variable achieves this, fortunately
#
GENODE_QMAKE_LIBS = \
	$(shell $(CC) $(CC_MARCH) -print-libgcc-file-name)

ifeq ($(CONTRIB_DIR),)
GENODE_QMAKE_INCDIR_OPENGL = $(call select_from_repositories,include/GL)/..
GENODE_QMAKE_INCDIR_EGL = $(call select_from_repositories,include/EGL)/..
else
GENODE_QMAKE_INCDIR_OPENGL := $(call select_from_ports,mesa)/include
GENODE_QMAKE_INCDIR_EGL := $(call select_from_ports,mesa)/include
endif

GENODE_QMAKE_LIBS_OPENGL = $(CURDIR)/build_dependencies/lib/mesa.lib.so
GENODE_QMAKE_LIBS_EGL = $(CURDIR)/build_dependencies/lib/egl.lib.so

#
# export variables for qmake.conf
#

env.sh:
	$(VERBOSE)rm -f $@
	$(VERBOSE)echo "export GENODE_QMAKE_CC='$(CC)'" >> $@
	$(VERBOSE)echo "export GENODE_QMAKE_CXX='$(CXX)'" >> $@
	$(VERBOSE)echo "export GENODE_QMAKE_LINK='$(CXX)'" >> $@
	$(VERBOSE)echo "export GENODE_QMAKE_AR='$(AR)'" >> $@
	$(VERBOSE)echo "export GENODE_QMAKE_OBJCOPY='$(OBJCOPY)'" >> $@
	$(VERBOSE)echo "export GENODE_QMAKE_NM='$(NM)'" >> $@
	$(VERBOSE)echo "export GENODE_QMAKE_STRIP='$(STRIP)'" >> $@
	$(VERBOSE)echo "export GENODE_QMAKE_CFLAGS='$(GENODE_QMAKE_CFLAGS)'" >> $@
	$(VERBOSE)echo "export GENODE_QMAKE_LFLAGS_APP='$(GENODE_QMAKE_LFLAGS_APP)'" >> $@
	$(VERBOSE)echo "export GENODE_QMAKE_LFLAGS_SHLIB='$(GENODE_QMAKE_LFLAGS_SHLIB)'" >> $@
	$(VERBOSE)echo "export GENODE_QMAKE_LIBS='$(GENODE_QMAKE_LIBS)'" >> $@
	$(VERBOSE)echo "export GENODE_QMAKE_INCDIR_OPENGL='$(GENODE_QMAKE_INCDIR_OPENGL)'" >> $@
	$(VERBOSE)echo "export GENODE_QMAKE_LIBS_OPENGL='$(GENODE_QMAKE_LIBS_OPENGL)'" >> $@
	$(VERBOSE)echo "export GENODE_QMAKE_INCDIR_EGL='$(GENODE_QMAKE_INCDIR_EGL)'" >> $@
	$(VERBOSE)echo "export GENODE_QMAKE_LIBS_EGL='$(GENODE_QMAKE_LIBS_EGL)'" >> $@


#
# prepare a directory named 'build_dependencies' where qmake can find needed files
#

build_dependencies/bin: build_dependencies
	$(VERBOSE)mkdir -p $@
	$(VERBOSE)ln -sf $(QT_TOOLS_DIR)/bin/* $@/

build_dependencies/mkspecs: build_dependencies
	$(VERBOSE)mkdir -p $@
	$(VERBOSE)ln -sf $(QT_API_DIR)/mkspecs/* $@/
	$(VERBOSE)rm -f $@/modules
	$(VERBOSE)mkdir $@/modules
	$(VERBOSE)ln -snf $(QT_API_DIR)/mkspecs/modules/* $@/modules/
	$(VERBOSE)ln -sf $(QT_PLATFORM)/qconfig.pri $@/
	$(VERBOSE)ln -sf $(QT_PLATFORM)/qmodule.pri $@/

qmake_prepared.tag: env.sh \
                    build_dependencies/bin \
                    build_dependencies/include \
                    build_dependencies/lib/libc.lib.so \
                    build_dependencies/lib/libm.lib.so \
                    build_dependencies/lib/egl.lib.so \
                    build_dependencies/lib/mesa.lib.so \
                    build_dependencies/lib/qt5_component.lib.so \
                    build_dependencies/lib/stdcxx.lib.so \
                    build_dependencies/lib/ldso_so_support.lib.a \
                    build_dependencies/mkspecs
	$(VERBOSE)touch $@

.PHONY: build_with_qmake

build_with_qmake: qmake_prepared.tag qt5_so_files

	$(VERBOSE)source env.sh && $(QMAKE) \
		-spec build_dependencies/mkspecs/$(QT_PLATFORM) \
		-qtconf build_dependencies/mkspecs/$(QT_PLATFORM)/qt.conf \
		-nocache \
		$(QMAKE_PROJECT_FILE) \
		"CONFIG += force_debug_info" \
		$(QT5_OUTPUT_FILTER)

	$(VERBOSE)source env.sh && $(MAKE) $(QT5_OUTPUT_FILTER)

	$(VERBOSE)for qmake_target_binary in $(QMAKE_TARGET_BINARIES); do \
		$(OBJCOPY) --only-keep-debug $${qmake_target_binary} $${qmake_target_binary}.debug; \
		$(STRIP) $${qmake_target_binary} -o $${qmake_target_binary}.stripped; \
		$(OBJCOPY) --add-gnu-debuglink=$${qmake_target_binary}.debug $${qmake_target_binary}.stripped; \
		ln -sf $(CURDIR)/$${qmake_target_binary}.stripped $(PWD)/bin/$${qmake_target_binary}; \
		ln -sf $(CURDIR)/$${qmake_target_binary}.stripped $(PWD)/debug/$${qmake_target_binary}; \
		ln -sf $(CURDIR)/$${qmake_target_binary}.debug $(PWD)/debug/; \
	done

BUILD_ARTIFACTS += $(notdir $(QMAKE_TARGET_BINARIES))

#
# build with qmake by default
#

QT5_TARGET_DEPS ?= build_with_qmake

TARGET ?= $(notdir $(QMAKE_PROJECT_FILE)).qmake_target
.PHONY: $(TARGET)
$(TARGET): $(QT5_TARGET_DEPS) $(QT5_EXTRA_TARGET_DEPS)
