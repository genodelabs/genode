include $(REP_DIR)/lib/import/import-qt_scriptclassic.mk

SHARED_LIB = yes

# extracted from src/script/Makefile
QT_DEFINES += -DQT_BUILD_SCRIPT_LIB -DQT_NO_USING_NAMESPACE -DQLALR_NO_QSCRIPTGRAMMAR_DEBUG_INFO -DQT_NO_CAST_TO_ASCII -DQT_ASCII_CAST_WARNINGS -DQT_MOC_COMPAT -DQ_SCRIPT_DIRECT_CODE -DQT_NO_DEBUG -DQT_CORE_LIB

# use default warning level to avoid noise when compiling contrib code
CC_WARN =

include $(REP_DIR)/lib/mk/qt_scriptclassic_generated.inc

INC_DIR += $(REP_DIR)/src/lib/qt4/mkspecs/qws/genode-x86-g++ \
           $(REP_DIR)/include/qt4 \
           $(REP_DIR)/contrib/include \
           $(REP_DIR)/include/qt4/QtCore \
           $(REP_DIR)/contrib/include/QtCore \
           $(REP_DIR)/include/qt4/QtCore/private \
           $(REP_DIR)/contrib/include/QtCore/private \
           $(REP_DIR)/include/qt4/QtScript \
           $(REP_DIR)/contrib/qtscriptclassic-1.0_1-opensource/include/QtScript \
           $(REP_DIR)/contrib/qtscriptclassic-1.0_1-opensource/src \
           $(REP_DIR)/src/lib/qt4/src/corelib/global

LIBS += qt_core libc

vpath % $(REP_DIR)/include/qt4/QtScript
vpath % $(REP_DIR)/include/qt4/QtScript/private

vpath % $(REP_DIR)/src/lib/qt4/src/script

vpath % $(REP_DIR)/contrib/qtscriptclassic-1.0_1-opensource/src

include $(REP_DIR)/lib/mk/qt.inc
