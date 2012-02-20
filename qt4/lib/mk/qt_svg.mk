include $(REP_DIR)/lib/import/import-qt_svg.mk

SHARED_LIB = yes

# extracted from src/svg/Makefile
QT_DEFINES += -DQT_BUILD_SVG_LIB -DQT_NO_USING_NAMESPACE -DQT_NO_CAST_TO_ASCII -DQT_ASCII_CAST_WARNINGS -DQT_MOC_COMPAT -DQT_NO_DEBUG -DQT_GUI_LIB -DQT_CORE_LIB

include $(REP_DIR)/lib/mk/qt_svg_generated.inc

INC_DIR += $(REP_DIR)/include/qt4/QtSvg/private \
           $(REP_DIR)/contrib/$(QT4)/include/QtSvg/private \
           $(REP_DIR)/contrib/$(QT4)/src/3rdparty/harfbuzz/src

LIBS += qt_core qt_gui zlib libc

vpath % $(REP_DIR)/include/qt4/QtSvg
vpath % $(REP_DIR)/include/qt4/QtSvg/private

vpath % $(REP_DIR)/src/lib/qt4/src/svg

vpath % $(REP_DIR)/contrib/$(QT4)/src/svg

include $(REP_DIR)/lib/mk/qt.inc
