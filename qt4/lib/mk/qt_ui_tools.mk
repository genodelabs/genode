include $(REP_DIR)/lib/import/import-qt_ui_tools.mk

SHARED_LIB = yes

# extracted from tools/designer/src/uitools/Makefile
QT_DEFINES += -DQFORMINTERNAL_NAMESPACE -DQT_DESIGNER_STATIC -DQT_FORMBUILDER_NO_SCRIPT -DQT_DESIGNER -DQT_NO_DEBUG -DQT_XML_LIB -DQT_GUI_LIB -DQT_CORE_LIB

CC_OPT += -Wno-unused-but-set-variable

include $(REP_DIR)/lib/mk/qt_ui_tools_generated.inc

INC_DIR += $(REP_DIR)/src/lib/qt4/tools/designer/src/lib/uilib \
           $(REP_DIR)/contrib/$(QT4)/tools/designer/src/lib/uilib \
           $(REP_DIR)/src/lib/qt4/tools/designer/src/uitools \
           $(REP_DIR)/contrib/$(QT4)/tools/designer/src/uitools

LIBS += qt_core qt_gui qt_xml libc

vpath % $(REP_DIR)/include/qt4/QtUiTools
vpath % $(REP_DIR)/include/qt4/QtUiTools/private

vpath % $(REP_DIR)/src/lib/qt4/tools/designer/src/lib/uilib
vpath % $(REP_DIR)/src/lib/qt4/tools/designer/src/uitools

vpath % $(REP_DIR)/contrib/$(QT4)/tools/designer/src/lib/uilib
vpath % $(REP_DIR)/contrib/$(QT4)/tools/designer/src/uitools

include $(REP_DIR)/lib/mk/qt.inc
