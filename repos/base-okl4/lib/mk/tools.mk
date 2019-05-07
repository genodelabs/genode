#
# Create symlink to elfweaver so that the run tool can use it from within the
# build directory.
#
OKL4_DIR    = $(call select_from_ports,okl4)/src/kernel/okl4
HOST_TOOLS += $(BUILD_BASE_DIR)/tool/okl4/elfweaver

$(BUILD_BASE_DIR)/tool/okl4/elfweaver:
	$(VERBOSE)mkdir -p $(dir $@)
	$(VERBOSE)cp -a $(OKL4_DIR)/tools/pyelf $(dir $@)/
	$(VERBOSE)ln -sf pyelf/elfweaver $@
