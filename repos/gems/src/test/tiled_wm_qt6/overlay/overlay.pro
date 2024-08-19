TEMPLATE   = app
TARGET     = test-tiled_wm-overlay
QT         = core gui widgets
CONFIG    += c++2a
SOURCES   += main.cpp overlay.cpp
HEADERS   += overlay.h ../util.h
RESOURCES  = overlay.qrc
