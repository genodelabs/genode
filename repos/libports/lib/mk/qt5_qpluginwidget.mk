SHARED_LIB = yes

SRC_CC   = qpluginwidget.cpp

HEADERS += qpluginwidget.h

vpath %.h $(REP_DIR)/include/qt5/qpluginwidget
vpath %.cpp $(REP_DIR)/src/lib/qt5/qpluginwidget

LIBS += qt5_gui qt5_widgets qt5_network qt5_qnitpickerviewwidget qt5_core qt5_qpa_nitpicker libc qoost zlib
