SHARED_LIB = yes

SRC_CC   = qnitpickerviewwidget.cpp

HEADERS += qnitpickerviewwidget.h

vpath %.h $(REP_DIR)/include/qnitpickerviewwidget
vpath %.cpp $(REP_DIR)/src/lib/qnitpickerviewwidget

LIBS += qt_gui libc
