INC_DIR += $(PRG_DIR)/qwebplugins/nitpicker
INC_DIR += $(call select_from_ports,arora)/src/app/arora/src/qwebplugins/nitpicker

INCLUDEPATH += $$PWD
DEPENDPATH += $$PWD

HEADERS += \
  nitpickerpluginwidget.h \
  nitpickerplugin.h

SOURCES += \
  nitpickerpluginwidget.cpp \
  nitpickerplugin.cpp
