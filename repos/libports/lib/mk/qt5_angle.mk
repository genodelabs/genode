include $(REP_DIR)/lib/import/import-qt5_angle.mk

SHARED_LIB = yes

#
# Generated files
#
# some of the following lines have been extracted from the console output
# of the 'configure' script (with modifications), that's why they can be
# quite long
#

ifneq ($(call select_from_ports,qt5),)
all: $(QT5_PORT_DIR)/src/lib/qt5/qtwebkit/Source/ThirdParty/ANGLE/generated/generated.tag
endif

ANGLE_DIR = $(QT5_CONTRIB_DIR)/qtwebkit/Source/ThirdParty/ANGLE

# make the 'HOST_TOOLS' variable known
include $(REP_DIR)/lib/mk/qt5_host_tools.mk

$(QT5_PORT_DIR)/src/lib/qt5/qtwebkit/Source/ThirdParty/ANGLE/generated/generated.tag: $(HOST_TOOLS)

	$(VERBOSE)mkdir -p $(dir $@)

	$(VERBOSE)flex --noline --nounistd --outfile=$(dir $@)/glslang_lex.cpp $(ANGLE_DIR)/src/compiler/glslang.l
	$(VERBOSE)flex --noline --nounistd --outfile=$(dir $@)/Tokenizer_lex.cpp $(ANGLE_DIR)/src/compiler/preprocessor/Tokenizer.l
	$(VERBOSE)bison --no-lines --skeleton=yacc.c --defines=$(dir $@)/glslang_tab.h --output=$(dir $@)/glslang_tab.cpp $(ANGLE_DIR)/src/compiler/glslang.y
	$(VERBOSE)bison --no-lines --skeleton=yacc.c --defines=$(dir $@)/ExpressionParser_tab.h --output=$(dir $@)/ExpressionParser_tab.cpp $(ANGLE_DIR)/src/compiler/preprocessor/ExpressionParser.y

	$(VERBOSE)touch $@

include $(REP_DIR)/lib/mk/qt5_angle_generated.inc

QT_INCPATH += qtwebkit/Source/ThirdParty/ANGLE/generated

QT_VPATH += qtwebkit/Source/ThirdParty/ANGLE/generated

include $(REP_DIR)/lib/mk/qt5.inc

LIBS += qt5_opengl
