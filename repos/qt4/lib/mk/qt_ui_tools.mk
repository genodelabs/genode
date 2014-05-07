include $(REP_DIR)/lib/import/import-qt_ui_tools.mk

SHARED_LIB = yes

CC_OPT += -Wno-unused-but-set-variable

include $(REP_DIR)/lib/mk/qt_ui_tools_generated.inc

include $(REP_DIR)/lib/mk/qt.inc

LIBS += qt_core qt_gui qt_xml libc

vpath % $(REP_DIR)/include/qt4/QtUiTools
vpath % $(REP_DIR)/include/qt4/QtUiTools/private

vpath % $(REP_DIR)/src/lib/qt4/tools/designer/src/lib/uilib
vpath % $(REP_DIR)/src/lib/qt4/tools/designer/src/uitools

vpath % $(REP_DIR)/contrib/$(QT4)/tools/designer/src/lib/uilib
vpath % $(REP_DIR)/contrib/$(QT4)/tools/designer/src/uitools
