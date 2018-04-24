include $(call select_from_repositories,lib/import/import-qt5_qnitpickerviewwidget.mk)

SHARED_LIB = yes

SRC_CC   = qnitpickerviewwidget.cpp

HEADERS += qnitpickerviewwidget.h

vpath %.h $(call select_from_repositories,include/qt5/qnitpickerviewwidget)
vpath %.cpp $(REP_DIR)/src/lib/qt5/qnitpickerviewwidget

LIBS += libc qoost qt5_core qt5_gui qt5_qpa_nitpicker qt5_widgets

CC_CXX_WARN_STRICT =
