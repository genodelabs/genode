# build with QtCore and QtGui support by default
# (can be overridden in the qmake project file)
QT = core gui

# find out the name of the project directory
PROJECT_DIR_NAME = $(notdir $(abspath $(PRG_DIR)))

INC_DIR += $(PRG_DIR)

###### start of editable part ######

# include the qmake project file
# -> change the filename if it differs from the project dir name

#QMAKE_PROJECT_FILE = $(realpath $(PRG_DIR)/$(PROJECT_DIR_NAME).pro)
QMAKE_PROJECT_FILE = $(realpath $(REP_DIR)/contrib/qt-everywhere-opensource-src-4.7.1/demos/textedit/textedit.pro)

ifneq ($(strip $(QMAKE_PROJECT_FILE)),)
include $(QMAKE_PROJECT_FILE)
endif

#
# The following files don't need to be moc'ed,
# so exclude them from $(SOURCES) and $(HEADERS).
#
NO_MOC_SOURCES = main.cpp textedit.cpp
SOURCES := $(filter-out $(NO_MOC_SOURCES), $(SOURCES))
SRC_CC   = $(NO_MOC_SOURCES)

# how to name the generated executable
# (if not already defined in the qmake project file)
# -> change if it shall not be the name of the project dir

ifndef TARGET
TARGET = $(PROJECT_DIR_NAME)
endif

CC_CXX_OPT += -DQT_MAIN_STACK_SIZE=512*1024

vpath % $(REP_DIR)/contrib/qt-everywhere-opensource-src-4.7.1/demos/textedit

###### end of editable part ######

LIBS += cxx libc

# static Qt plugins
ifeq ($(findstring qjpeg, $(QT_PLUGIN)), qjpeg)
LIBS += qjpeg
endif

# QtCore stuff
ifeq ($(findstring core, $(QT)), core)
QT_DEFINES += -DQT_CORE_LIB
LIBS += qt_core
endif

# QtGui stuff
ifeq ($(findstring gui, $(QT)), gui)
QT_DEFINES += -DQT_GUI_LIB
LIBS += qt_gui dejavusans
endif

# QtNetwork stuff
ifeq ($(findstring network, $(QT)), network)
LIBS += qt_network
endif

# QtScript stuff
ifeq ($(findstring script, $(QT)), script)
LIBS += qt_script
endif

# QtScriptTools stuff
ifeq ($(findstring scripttools, $(QT)), scripttools)
LIBS += qt_scripttools
endif

# QtSvg stuff
ifeq ($(findstring svg, $(QT)), svg)
LIBS += qt_svg
endif

# QtXml stuff
ifeq ($(findstring xml, $(QT)), xml)
LIBS += qt_xml
endif

# QtUiTools stuff
ifeq ($(findstring uitools, $(CONFIG)), uitools)
LIBS += qt_ui_tools
endif

# QtWebKit stuff
ifeq ($(findstring webkit, $(QT)), webkit)
LIBS += qt_webcore
endif

#endif # $(QMAKE_PROJECT_FILE)
