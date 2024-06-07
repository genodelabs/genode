TEMPLATE   = app
TARGET     = test-tiled_wm-app
QT         = core gui widgets
CONFIG    += c++2a
SOURCES   += main.cpp app.cpp
HEADERS   += app.h ../util.h
RESOURCES  = app.qrc
