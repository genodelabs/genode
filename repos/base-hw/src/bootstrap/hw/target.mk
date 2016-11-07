TARGET        = bootstrap
LIBS          = bootstrap-hw
BOOTSTRAP_OBJ = bootstrap.o

$(TARGET): $(BOOTSTRAP_OBJ)
	$(VERBOSE)true

.PHONY: $(BOOTSTRAP_OBJ)
$(BOOTSTRAP_OBJ):
	$(VERBOSE)$(LD) $(LD_MARCH) -u _start --whole-archive -r $(LINK_ITEMS) $(LIBCXX_GCC) -o $@

clean cleanall:
	$(VERBOSE)rm -f $(BOOTSTRAP_OBJ)
