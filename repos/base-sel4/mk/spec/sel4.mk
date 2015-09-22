LD_TEXT_ADDR ?= 0x01000000

#
# Clean rule for removing the side effects of building the platform library
#
clean_includes:
	$(VERBOSE)rm -rf $(BUILD_BASE_DIR)/include

clean cleanall: clean_includes
