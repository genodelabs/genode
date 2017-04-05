TARGET    = core
CORE_OBJ ?= core.o

$(TARGET): $(CORE_OBJ)
	$(VERBOSE)ln -sf $(CURDIR)/$(CORE_OBJ) $(INSTALL_DIR)/$(CORE_OBJ)

.PHONY: $(CORE_OBJ)
$(CORE_OBJ):
	$(VERBOSE)$(LD) $(LD_MARCH) -u _start --whole-archive -r $(LINK_ITEMS) $(LIBCXX_GCC) -o $@

clean cleanall:
	$(VERBOSE)rm -f $(CORE_OBJ)
