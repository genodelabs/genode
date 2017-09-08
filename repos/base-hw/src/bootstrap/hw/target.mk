TARGET        = bootstrap
LIBS          = bootstrap-hw
BOOTSTRAP_OBJ = bootstrap-hw.o

$(TARGET):
	$(VERBOSE)true

ifneq ($(INSTALL_DIR),)
ifneq ($(DEBUG_DIR),)
$(TARGET): $(INSTALL_DIR)/$(BOOTSTRAP_OBJ)

$(BOOTSTRAP_OBJ).stripped: $(BOOTSTRAP_OBJ)
	$(VERBOSE)$(STRIP) --strip-debug -o $@ $<

$(INSTALL_DIR)/$(BOOTSTRAP_OBJ) : $(BOOTSTRAP_OBJ).stripped
	$(VERBOSE)ln -sf $(CURDIR)/$< $@

$(DEBUG_DIR)/$(BOOTSTRAP_OBJ) : $(BOOTSTRAP_OBJ)
	$(VERBOSE)ln -sf $(CURDIR)/$(BOOTSTRAP_OBJ) $(DEBUG_DIR)/$(BOOTSTRAP_OBJ)
endif
endif

.PHONY: $(BOOTSTRAP_OBJ)
$(BOOTSTRAP_OBJ):
	$(VERBOSE)$(LD) $(LD_MARCH) -u _start --whole-archive -r $(LINK_ITEMS) $(LIBCXX_GCC) -o $@

clean cleanall:
	$(VERBOSE)rm -f $(BOOTSTRAP_OBJ) $(BOOTSTRAP_OBJ).stripped
