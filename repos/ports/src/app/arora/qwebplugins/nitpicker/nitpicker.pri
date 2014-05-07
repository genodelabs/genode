INC_DIR += $(PRG_DIR)/qwebplugins/nitpicker
INC_DIR += $(REP_DIR)/contrib/$(ARORA)/src/qwebplugins/nitpicker

INCLUDEPATH += $$PWD
DEPENDPATH += $$PWD

HEADERS += \
  nitpickerpluginwidget.h \
  nitpickerplugin.h

SOURCES += \
  nitpickerpluginwidget.cpp \
  nitpickerplugin.cpp
