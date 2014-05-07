include $(REP_DIR)/lib/import/import-qt_scripttools.mk

SHARED_LIB = yes

include $(REP_DIR)/lib/mk/qt_scripttools_generated.inc

include $(REP_DIR)/lib/mk/qt.inc

INC_DIR += $(REP_DIR)/include/qt4/QtScriptTools/private \
           $(REP_DIR)/contrib/$(QT4)/include/QtScriptTools/private

LIBS += qt_core libc

vpath % $(REP_DIR)/include/qt4/QtScriptTools
vpath % $(REP_DIR)/include/qt4/QtScriptTools/private
