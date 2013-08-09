include $(REP_DIR)/lib/import/import-qt5_scriptclassic.mk

SHARED_LIB = yes

# use default warning level to avoid noise when compiling contrib code
CC_WARN =

include $(REP_DIR)/lib/mk/qt5_scriptclassic_generated.inc

include $(REP_DIR)/lib/mk/qt5.inc

INC_DIR += $(REP_DIR)/src/lib/qt5/qtbase/mkspecs/qws/genode-generic-g++ \
           $(REP_DIR)/include/qt5 \
           $(REP_DIR)/contrib/include \
           $(REP_DIR)/include/qt5/qtbase/QtCore \
           $(REP_DIR)/contrib/qtbase/include/QtCore \
           $(REP_DIR)/include/qt5/QtCore/private \
           $(REP_DIR)/contrib/qtbase/include/QtCore/private \
           $(REP_DIR)/include/qt5/QtScript \
           $(REP_DIR)/contrib/qtscriptclassic-1.0_1-opensource/include/QtScript \
           $(REP_DIR)/contrib/qtscriptclassic-1.0_1-opensource/src \
           $(REP_DIR)/src/lib/qt5/qtbase/src/corelib/global

LIBS += qt5_core libc

vpath % $(REP_DIR)/include/qt5/QtScript
vpath % $(REP_DIR)/include/qt5/QtScript/private

vpath % $(REP_DIR)/src/lib/qt5/qtbase/src/script

vpath % $(REP_DIR)/contrib/qtscriptclassic-1.0_1-opensource/src
