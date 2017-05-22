include $(REP_DIR)/lib/import/import-qt5_jscore.mk

SHARED_LIB = yes

# additional defines for the Genode version
CC_OPT += -DSQLITE_NO_SYNC=1 -DSQLITE_THREADSAFE=0

# enable C++ functions that use C99 math functions (disabled by default in the Genode tool chain)
CC_CXX_OPT += -D_GLIBCXX_USE_C99_MATH

# use default warning level to avoid noise when compiling contrib code
CC_WARN =

#
# Generated files
#
# some of the following lines have been extracted from the console output
# of the 'configure' script (with modifications), that's why they can be
# quite long
#

ifneq ($(call select_from_ports,qt5),)
all: $(QT5_PORT_DIR)/src/lib/qt5/qtwebkit/Source/JavaScriptCore/generated/generated.tag
endif

JAVASCRIPTCORE_DIR = $(QT5_CONTRIB_DIR)/qtwebkit/Source/JavaScriptCore

$(QT5_PORT_DIR)/src/lib/qt5/qtwebkit/Source/JavaScriptCore/generated/generated.tag:

	$(VERBOSE)mkdir -p $(dir $@)

	@# create_hash_table
	$(VERBOSE)perl $(JAVASCRIPTCORE_DIR)/create_hash_table $(JAVASCRIPTCORE_DIR)/runtime/ArrayConstructor.cpp -i  > $(dir $@)/ArrayConstructor.lut.h
	$(VERBOSE)perl $(JAVASCRIPTCORE_DIR)/create_hash_table $(JAVASCRIPTCORE_DIR)/runtime/ArrayPrototype.cpp -i    > $(dir $@)/ArrayPrototype.lut.h
	$(VERBOSE)perl $(JAVASCRIPTCORE_DIR)/create_hash_table $(JAVASCRIPTCORE_DIR)/runtime/BooleanPrototype.cpp -i  > $(dir $@)/BooleanPrototype.lut.h
	$(VERBOSE)perl $(JAVASCRIPTCORE_DIR)/create_hash_table $(JAVASCRIPTCORE_DIR)/runtime/DateConstructor.cpp -i   > $(dir $@)/DateConstructor.lut.h
	$(VERBOSE)perl $(JAVASCRIPTCORE_DIR)/create_hash_table $(JAVASCRIPTCORE_DIR)/runtime/DatePrototype.cpp -i     > $(dir $@)/DatePrototype.lut.h
	$(VERBOSE)perl $(JAVASCRIPTCORE_DIR)/create_hash_table $(JAVASCRIPTCORE_DIR)/runtime/ErrorPrototype.cpp -i    > $(dir $@)/ErrorPrototype.lut.h
	$(VERBOSE)perl $(JAVASCRIPTCORE_DIR)/create_hash_table $(JAVASCRIPTCORE_DIR)/runtime/JSGlobalObject.cpp -i    > $(dir $@)/JSGlobalObject.lut.h
	$(VERBOSE)perl $(JAVASCRIPTCORE_DIR)/create_hash_table $(JAVASCRIPTCORE_DIR)/runtime/JSONObject.cpp -i        > $(dir $@)/JSONObject.lut.h
	$(VERBOSE)perl $(JAVASCRIPTCORE_DIR)/create_hash_table $(JAVASCRIPTCORE_DIR)/runtime/MathObject.cpp -i        > $(dir $@)/MathObject.lut.h
	$(VERBOSE)perl $(JAVASCRIPTCORE_DIR)/create_hash_table $(JAVASCRIPTCORE_DIR)/runtime/NamePrototype.cpp -i     > $(dir $@)/NamePrototype.lut.h
	$(VERBOSE)perl $(JAVASCRIPTCORE_DIR)/create_hash_table $(JAVASCRIPTCORE_DIR)/runtime/NumberConstructor.cpp -i > $(dir $@)/NumberConstructor.lut.h
	$(VERBOSE)perl $(JAVASCRIPTCORE_DIR)/create_hash_table $(JAVASCRIPTCORE_DIR)/runtime/NumberPrototype.cpp -i   > $(dir $@)/NumberPrototype.lut.h
	$(VERBOSE)perl $(JAVASCRIPTCORE_DIR)/create_hash_table $(JAVASCRIPTCORE_DIR)/runtime/ObjectConstructor.cpp -i > $(dir $@)/ObjectConstructor.lut.h
	$(VERBOSE)perl $(JAVASCRIPTCORE_DIR)/create_hash_table $(JAVASCRIPTCORE_DIR)/runtime/RegExpConstructor.cpp -i > $(dir $@)/RegExpConstructor.lut.h
	$(VERBOSE)perl $(JAVASCRIPTCORE_DIR)/create_hash_table $(JAVASCRIPTCORE_DIR)/runtime/RegExpObject.cpp -i      > $(dir $@)/RegExpObject.lut.h
	$(VERBOSE)perl $(JAVASCRIPTCORE_DIR)/create_hash_table $(JAVASCRIPTCORE_DIR)/runtime/RegExpPrototype.cpp -i   > $(dir $@)/RegExpPrototype.lut.h
	$(VERBOSE)perl $(JAVASCRIPTCORE_DIR)/create_hash_table $(JAVASCRIPTCORE_DIR)/runtime/StringConstructor.cpp -i > $(dir $@)/StringConstructor.lut.h
	$(VERBOSE)perl $(JAVASCRIPTCORE_DIR)/create_hash_table $(JAVASCRIPTCORE_DIR)/parser/Keywords.table -i         > $(dir $@)/Lexer.lut.h

	@# create_regex_tables 
	$(VERBOSE)python $(JAVASCRIPTCORE_DIR)/create_regex_tables > $(dir $@)/RegExpJitTables.h

	@# KeywordLookupGenerator.py
	$(VERBOSE)python $(JAVASCRIPTCORE_DIR)/KeywordLookupGenerator.py $(JAVASCRIPTCORE_DIR)/parser/Keywords.table  > $(dir $@)/KeywordLookup.h

	$(VERBOSE)touch $@


include $(REP_DIR)/lib/mk/qt5_jscore_generated.inc

QT_INCPATH += qtwebkit/Source/JavaScriptCore/generated


include $(REP_DIR)/lib/mk/qt5.inc

LIBS += qt5_network qt5_core icu pthread libc libm
