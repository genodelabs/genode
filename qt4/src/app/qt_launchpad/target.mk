# build with QtCore and QtGui support by default
# (can be overridden in the qmake project file)
QT = core gui

# find out the name of the project directory
PROJECT_DIR_NAME = $(notdir $(abspath $(PRG_DIR)))

INC_DIR += $(PRG_DIR)

SRC_CC = child_entry.cpp \
         kbyte_loadbar.cpp \
         launch_entry.cpp \
         main.cpp \
         qt_launchpad.cpp

###### start of editable part ######

# include the qmake project file
# -> change the filename if it differs from the project dir name

QMAKE_PROJECT_FILE = $(realpath $(PRG_DIR)/$(PROJECT_DIR_NAME).pro)

ifneq ($(strip $(QMAKE_PROJECT_FILE)),)
include $(QMAKE_PROJECT_FILE)
endif

# how to name the generated executable
# (if not already defined in the qmake project file)
# -> change if it shall not be the name of the project dir

ifndef TARGET
TARGET = $(PROJECT_DIR_NAME)
endif

CC_CXX_OPT += -DQT_MAIN_STACK_SIZE=512*1024

LIBS += cxx server process launchpad

###### end of editable part ######

LIBS += libc

# QtCore stuff
ifeq ($(findstring core, $(QT)), core)
QT_DEFINES += -DQT_CORE_LIB
LIBS += qt_core
endif

# QtGui stuff
ifeq ($(findstring gui, $(QT)), gui)
QT_DEFINES += -DQT_GUI_LIB
LIBS += qt_gui
ifneq ($(findstring font.qrc, $(RESOURCES)), font.qrc)
RESOURCES += font.qrc
endif
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

#endif # $(QMAKE_PROJECT_FILE)
