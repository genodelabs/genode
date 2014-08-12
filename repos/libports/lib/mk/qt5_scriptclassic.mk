include $(REP_DIR)/lib/import/import-qt5_scriptclassic.mk

SHARED_LIB = yes

# use default warning level to avoid noise when compiling contrib code
CC_WARN =

include $(REP_DIR)/lib/mk/qt5_scriptclassic_generated.inc

include $(REP_DIR)/lib/mk/qt5.inc

LIBS += qt5_core libc

vpath % $(QT5_PORT_DIR)/src/lib/qt5/qtscriptclassic-1.0_1-opensource/src
