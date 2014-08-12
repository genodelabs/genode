SHARED_LIB = yes

SRC_CC   = qnitpickerviewwidget.cpp

HEADERS += qnitpickerviewwidget.h

vpath %.h $(REP_DIR)/include/qt5/qnitpickerviewwidget
vpath %.cpp $(REP_DIR)/src/lib/qt5/qnitpickerviewwidget

LIBS += qt5_gui qt5_widgets qt5_core libc qt5_qpa_nitpicker qoost
