REP_INC_DIR += src/lib/hw/$(ARCH_WIDTH_PATH)

SRC_CC += $(ARCH_WIDTH_PATH)/memory_map.cc

hw/memory_consts.h: $(call select_from_repositories,src/lib/hw/$(ARCH_WIDTH_PATH))/memory_consts.s $(MAKEFILE_LIST)
	$(MSG_CONVERT)$@
	$(VERBOSE)mkdir -p $(dir $@)
	$(VERBOSE)sed -e 's/^HW_MM_\([0-9A-Z_]*\)\s*=\s*\(.*\)/		static constexpr Genode::size_t \1 = \2;/' -e 's/^#//' $< > $@

$(SRC_CC:.cc=.o) $(SRC_S:.s=.o): hw/memory_consts.h

vpath hw/memory_consts.h $(LIB_CACHE_DIR)/$(LIB)
vpath $(ARCH_WIDTH_PATH)/memory_map.cc $(call select_from_repositories,src/lib/hw)
