#
# Create mirror for architecture-specific L4 header files
#
OKL4_INCLUDE_SYMLINKS = include/l4/arch

include $(REP_DIR)/lib/mk/syscall-okl4.inc

include/l4/arch: $(OKL4_DIR)/arch/ia32/libs/l4/include
	$(VERBOSE)ln -sf $< $@
