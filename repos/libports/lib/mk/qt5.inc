QT_SOURCES_FILTER_OUT += $(COMPILER_MOC_HEADER_MAKE_ALL_FILES_FILTER_OUT)
QT_SOURCES_FILTERED = $(filter-out $(QT_SOURCES_FILTER_OUT), $(QT_SOURCES))

SRC_CC += $(filter %.cpp, $(QT_SOURCES_FILTERED))
SRC_CC += $(filter %.cc,  $(QT_SOURCES_FILTERED))
SRC_C  += $(filter %.c,   $(QT_SOURCES_FILTERED))

COMPILER_MOC_HEADER_MAKE_ALL_FILES_FILTERED = $(filter-out $(COMPILER_MOC_HEADER_MAKE_ALL_FILES_FILTER_OUT), $(COMPILER_MOC_HEADER_MAKE_ALL_FILES))
$(addsuffix .o,$(basename $(SRC_CC))): $(COMPILER_MOC_HEADER_MAKE_ALL_FILES_FILTERED)

COMPILER_MOC_SOURCE_MAKE_ALL_FILES_FILTERED = $(filter-out $(COMPILER_MOC_SOURCE_MAKE_ALL_FILES_FILTER_OUT), $(COMPILER_MOC_SOURCE_MAKE_ALL_FILES))
$(addsuffix .o,$(basename $(SRC_CC))): $(COMPILER_MOC_SOURCE_MAKE_ALL_FILES_FILTERED)

QT5_INC_DIR += $(REP_DIR)/src/lib/qt5/qtbase/mkspecs/qws/genode-generic-g++ \
               $(REP_DIR)/src/lib/qt5/qtbase/src/corelib/global

QT5_INC_DIR += $(addprefix $(REP_DIR)/src/lib/qt5/, $(QT_INCPATH))
QT5_INC_DIR += $(addprefix $(REP_DIR)/contrib/$(QT5)/, $(QT_INCPATH))

vpath % $(addprefix $(REP_DIR)/src/lib/qt5/, $(QT_VPATH))
vpath % $(addprefix $(REP_DIR)/contrib/$(QT5)/, $(QT_VPATH))
