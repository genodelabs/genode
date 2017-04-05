TARGET    = core
CORE_OBJ ?= core.o

$(TARGET):
	@true

ifneq ($(INSTALL_DIR),)
ifneq ($(DEBUG_DIR),)
$(TARGET): $(INSTALL_DIR)/$(CORE_OBJ) $(DEBUG_DIR)/$(CORE_OBJ)

$(CORE_OBJ).stripped: $(CORE_OBJ)
	$(VERBOSE)$(STRIP) --strip-debug -o $@ $<

$(INSTALL_DIR)/$(CORE_OBJ) : $(CORE_OBJ).stripped
	$(VERBOSE)ln -sf $(CURDIR)/$< $(INSTALL_DIR)/$(CORE_OBJ)

$(DEBUG_DIR)/$(CORE_OBJ) : $(CORE_OBJ)
	$(VERBOSE)ln -sf $(CURDIR)/$< $(DEBUG_DIR)/$(CORE_OBJ)
endif
endif

.PHONY: $(CORE_OBJ)
$(CORE_OBJ):
	$(VERBOSE)$(LD) $(LD_MARCH) -u _start --whole-archive -r $(LINK_ITEMS) $(LIBCXX_GCC) -o $@

clean cleanall:
	$(VERBOSE)rm -f $(CORE_OBJ) $(CORE_OBJ).stripped
