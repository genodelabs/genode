INC_DIR += $(PRG_DIR)

LIBS += libc

# set the stack size of the main thread
CC_CXX_OPT += -DQT_MAIN_STACK_SIZE=$(QT_MAIN_STACK_SIZE)

# QtCore
ifeq ($(findstring core, $(QT)), core)
QT_DEFINES += -DQT_CORE_LIB
LIBS += qt5_core
endif

# QtGui
ifeq ($(findstring gui, $(QT)), gui)
QT_DEFINES += -DQT_GUI_LIB
LIBS += qt5_gui qt5_qpa_nitpicker qt5_widgets qt5_qjpeg
endif

# QtNetwork
ifeq ($(findstring network, $(QT)), network)
LIBS += qt5_network
endif

# QtPrintSupport
ifeq ($(findstring printsupport, $(QT)), printsupport)
LIBS += qt5_printsupport
endif

# QtQuick
ifeq ($(findstring quick, $(QT)), quick)
LIBS += qt5_quick
endif

# QtScript
ifeq ($(findstring scriptclassic, $(QT)), scriptclassic)
LIBS += qt5_scriptclassic
else
ifeq ($(findstring script, $(QT)), script)
# qt5_script does dot work very well at this time, so qt5_scriptclassic gets used in any case
LIBS += qt5_scriptclassic
endif
endif

# QtXml
ifeq ($(findstring xml, $(QT)), xml)
LIBS += qt5_xml
endif

# Qml
ifeq ($(findstring qml, $(QT)), qml)
LIBS += qt5_qml
endif

# QtUiTools
# Qt documentation says: CONFIG += uitools
ifeq ($(findstring uitools, $(CONFIG)), uitools)
LIBS += qt5_ui_tools
endif
# Qt5 documentation says: QT += uitools
ifeq ($(findstring uitools, $(QT)), uitools)
LIBS += qt5_ui_tools
endif

# QtWebKit
ifeq ($(findstring webkit, $(QT)), webkit)
LIBS += qt5_webcore qt5_webkit qt5_webkitwidgets
endif
