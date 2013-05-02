INC_DIR += $(PRG_DIR)

LIBS += libc

# set the stack size of the main thread
CC_CXX_OPT += -DQT_MAIN_STACK_SIZE=$(QT_MAIN_STACK_SIZE)

# static Qt plugins
ifeq ($(findstring qgif, $(QT_PLUGIN)), qgif)
LIBS += qgif
endif
ifeq ($(findstring qjpeg, $(QT_PLUGIN)), qjpeg)
LIBS += qjpeg
endif

# QtCore
ifeq ($(findstring core, $(QT)), core)
QT_DEFINES += -DQT_CORE_LIB
LIBS += qt_core
endif

# QtGui
ifeq ($(findstring gui, $(QT)), gui)
QT_DEFINES += -DQT_GUI_LIB
LIBS += qt_gui dejavusans
endif

# QtNetwork
ifeq ($(findstring network, $(QT)), network)
LIBS += qt_network
endif

# QtScript
ifeq ($(findstring script, $(QT)), script)
LIBS += qt_script
endif

# QtScriptTools
ifeq ($(findstring scripttools, $(QT)), scripttools)
LIBS += qt_scripttools
endif

# QtSvg
ifeq ($(findstring svg, $(QT)), svg)
LIBS += qt_svg
endif

# QtXml
ifeq ($(findstring xml, $(QT)), xml)
LIBS += qt_xml
endif

# QtUiTools
ifeq ($(findstring uitools, $(CONFIG)), uitools)
LIBS += qt_ui_tools
endif

# QtWebKit
ifeq ($(findstring webkit, $(QT)), webkit)
LIBS += qt_webcore qt_webkit
endif
