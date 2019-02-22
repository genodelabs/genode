include $(call select_from_repositories,lib/import/import-qt5_qpluginwidget.mk)

SHARED_LIB = yes

SRC_CC = qpluginwidget.cpp \
         moc_qpluginwidget.cpp

vpath %.h $(call select_from_repositories,include/qt5/qpluginwidget)
vpath %.cpp $(REP_DIR)/src/lib/qt5/qpluginwidget

LIBS += libc qoost qt5_core qt5_gui qt5_network qt5_qnitpickerviewwidget qt5_qpa_nitpicker qt5_widgets zlib

CC_CXX_WARN_STRICT =
