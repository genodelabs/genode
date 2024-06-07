TEMPLATE   = app
TARGET     = qt_launchpad
QT         = core gui widgets
CONFIG    += c++2a
HEADERS   += child_entry.h \
             kbyte_loadbar.h \
             launch_entry.h \
             qt_launchpad.h
SOURCES   += child_entry.cpp \
             kbyte_loadbar.cpp \
             launch_entry.cpp \
             main.cpp \
             qt_launchpad.cpp
FORMS     += child_entry.ui \
             launch_entry.ui \
             qt_launchpad.ui
