TEMPLATE   = app
TARGET     = test-tiled_wm-panel
QT         = core gui widgets
CONFIG    += c++2a
SOURCES   += main.cpp panel.cpp
HEADERS   += panel.h icon.h ../util.h
RESOURCES  = panel.qrc

INCLUDEPATH = ..
