#
# Execute the rules in this file only at the second build stage when we know
# about the complete build settings, e.g., the 'CROSS_DEV_PREFIX'.
#
ifeq ($(called_from_lib_mk),yes)

include $(REP_DIR)/lib/import/import-syscall-okl4.mk

#
# Make OKL4 kernel API headers available to the Genode build system
#
# We have to create a symbolic link of OKL's 'include/l4' directory into our
# local build directory. If we just added the original 'iguana/include'
# directory to the include-search locations, we would pollute the include
# search space with all Iguana include files, not just the OKL4 API includes.
#

OKL4_L4_INCLUDES = arch.h cache.h caps.h config.h interrupt.h ipc.h kdebug.h \
                   macros.h map.h memregion.h message.h misc.h mutex.h \
                   pagefault.h procdesc.h profile.h schedule.h security.h \
                   space.h thread.h time.h types.h utcb.h

OKL4_INCLUDE_SYMLINKS += $(addprefix include/l4/,$(OKL4_L4_INCLUDES))
OKL4_INCLUDE_SYMLINKS += $(addprefix include/,compat bootinfo)

OKL4_INCLUDE_DIRS = $(sort $(dir $(OKL4_INCLUDE_SYMLINKS)))

# make sure to create the 'include/l4' directory before the symbolic links
all: $(OKL4_INCLUDE_SYMLINKS)

$(OKL4_INCLUDE_SYMLINKS): $(filter-out $(wildcard $(OKL4_INCLUDE_DIRS)), $(OKL4_INCLUDE_DIRS))

$(OKL4_INCLUDE_DIRS):
	$(VERBOSE)mkdir -p $@

$(OKL4_DIR):
	$(VERBOSE)$(ECHO) "--> Please, execute './tool/ports/prepare_port okl4' in $(GENODE_DIR)"
	$(VERBOSE)$(ECHO) "--> before compiling Genode apps for OKL4."
	$(VERBOSE)exit 1

$(OKL4_DIR)/%: $(filter-out $(wildcard $(OKL4_DIR)), $(OKL4_DIR))

include/l4/%.h: $(OKL4_DIR)/libs/l4/include/%.h
	$(VERBOSE)ln -sf $< $@

include/compat: $(OKL4_DIR)/libs/compat/include/compat
	$(VERBOSE)ln -sf $< $@

include/bootinfo: $(OKL4_DIR)/libs/bootinfo/include
	$(VERBOSE)ln -sf $< $@

endif

#
# Create symlink to elfweaver so that the run tool can use it from within the
# build directory.
#
HOST_TOOLS += $(BUILD_BASE_DIR)/tool/okl4/elfweaver

$(BUILD_BASE_DIR)/tool/okl4/elfweaver:
	mkdir -p $(dir $@)
	ln -sf $(OKL4_DIR)/tools/pyelf/elfweaver $@

