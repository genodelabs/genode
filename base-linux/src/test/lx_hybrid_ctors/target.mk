TARGET = test-lx_hybrid_ctors
SRC_CC = main.cc
LIBS   = env lx_hybrid

EXT_OBJECTS += $(BUILD_BASE_DIR)/test/lx_hybrid_ctors/libtestlib.so

TESTLIB_SO     = libtestlib.so
TESTLIB_SRC_CC = testlib.cc

$(TARGET): libtestlib.so

$(TESTLIB_SO): $(TESTLIB_SRC_CC)
	$(MSG_BUILD)$(TESTLIB_SO)
	$(VERBOSE)g++ -fPIC -c $^
	$(VERBOSE)g++ -shared -Wlsoname,$(TESTLIB_SO) -o $@ $(notdir $(^:.cc=.o))

clean_libtestlib:
	$(VERBOSE)rm -f $(TESTLIB_SO) $(TESTLIB_SRC_CC:.cc=.o)

clean: clean_libtestlib

vpath testlib.cc $(PRG_DIR)
