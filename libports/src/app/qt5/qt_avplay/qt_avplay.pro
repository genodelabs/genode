TEMPLATE   = app
TARGET     = qt_avplay
QT         = core gui xml
HEADERS    = avplay_policy.h \
             control_bar.h \
             main_window.h
SOURCES    = control_bar.cpp \
             framebuffer_session_component.cc \
             input_service.cpp \
             main.cpp \
             main_window.cpp
RESOURCES  = style.qrc
