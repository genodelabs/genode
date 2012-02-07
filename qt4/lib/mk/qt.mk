SRC_CC += $(filter %.cpp, $(QT_SOURCES))
SRC_C += $(filter %.c, $(QT_SOURCES))

$(SRC_CC:.cpp=.o): $(COMPILER_MOC_HEADER_MAKE_ALL_FILES)
$(SRC_CC:.cpp=.o): $(COMPILER_MOC_SOURCE_MAKE_ALL_FILES)
