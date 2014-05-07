TARGET = test-lx_hybrid_ctors
SRC_CC = main.cc
LIBS   = lx_hybrid

TESTLIB_SO     = libtestlib.so
TESTLIB_SRC_CC = testlib.cc

EXT_OBJECTS += $(BUILD_BASE_DIR)/test/lx_hybrid_ctors/$(TESTLIB_SO)

$(TARGET): $(TESTLIB_SO)

$(TESTLIB_SO): $(TESTLIB_SRC_CC)
	$(MSG_BUILD)$(TESTLIB_SO)
	$(VERBOSE)g++ $(CC_MARCH) -fPIC -c $^
	$(VERBOSE)g++ $(CC_MARCH) -shared -o $@ $(notdir $(^:.cc=.o))

clean_libtestlib:
	$(VERBOSE)rm -f $(TESTLIB_SO) $(TESTLIB_SRC_CC:.cc=.o)

clean: clean_libtestlib

vpath testlib.cc $(PRG_DIR)
