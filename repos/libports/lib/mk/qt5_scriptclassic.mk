include $(call select_from_repositories,lib/import/import-qt5_scriptclassic.mk)

SHARED_LIB = yes

# use default warning level to avoid noise when compiling contrib code
CC_WARN =

include $(REP_DIR)/lib/mk/qt5_scriptclassic_generated.inc

include $(REP_DIR)/lib/mk/qt5.inc

LIBS += libc qt5_core qt5_gui qt5_widgets

INC_DIR += $(QT5_PORT_DIR)/src/lib/qt5/qtscriptclassic-1.0_1-opensource/src

vpath % $(QT5_PORT_DIR)/src/lib/qt5/qtscriptclassic-1.0_1-opensource/src

CC_CXX_WARN_STRICT =
