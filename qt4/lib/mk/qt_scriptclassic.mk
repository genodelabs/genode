include $(REP_DIR)/lib/import/import-qt_scriptclassic.mk

SHARED_LIB = yes

# use default warning level to avoid noise when compiling contrib code
CC_WARN =

include $(REP_DIR)/lib/mk/qt_scriptclassic_generated.inc

include $(REP_DIR)/lib/mk/qt.inc

INC_DIR += $(REP_DIR)/src/lib/qt4/mkspecs/qws/genode-generic-g++ \
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
