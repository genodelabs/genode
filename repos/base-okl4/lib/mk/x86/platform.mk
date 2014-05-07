#
# Create prerequisites for building Genode for OKL4
#
# Prior building Genode programs for OKL4, the kernel bindings needed are
# symlinked to the build directory.
#

#
# Create mirror for architecture-specific L4 header files
#
OKL4_INCLUDE_SYMLINKS = $(BUILD_BASE_DIR)/include/l4/arch

include $(REP_DIR)/lib/mk/platform.inc

$(BUILD_BASE_DIR)/include/l4/arch: $(OKL4_DIR)/arch/ia32/libs/l4/include
	$(VERBOSE)ln -sf $< $@
