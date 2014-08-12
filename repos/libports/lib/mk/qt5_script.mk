include $(REP_DIR)/lib/import/import-qt5_script.mk

SHARED_LIB = yes

# use default warning level to avoid noise when compiling contrib code
CC_WARN =

include $(REP_DIR)/lib/mk/qt5_script_generated.inc

QT_INCPATH += qtscript/src/script/api \

# remove unneeded files to prevent moc warnings
COMPILER_MOC_HEADER_MAKE_ALL_FILES_FILTER_OUT = \

COMPILER_MOC_SOURCE_MAKE_ALL_FILES_FILTER_OUT = \

include $(REP_DIR)/lib/mk/qt5.inc

INC_DIR += $(QT5_CONTRIB_DIR)/qtscript/include/QtScript/$(QT_VERSION)/QtScript \
           $(QT5_CONTRIB_DIR)/qtbase/include/QtCore/$(QT_VERSION)/QtCore \

LIBS += qt5_core pthread

# see https://github.com/genodelabs/genode/issues/890
REQUIRES += cxx11_fix
