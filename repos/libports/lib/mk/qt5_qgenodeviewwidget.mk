include $(call select_from_repositories,lib/import/import-qt5_qgenodeviewwidget.mk)

SHARED_LIB = yes

SRC_CC = qgenodeviewwidget.cpp \
         moc_qgenodeviewwidget.cpp

vpath %.h $(call select_from_repositories,include/qt5/qgenodeviewwidget)
vpath %.cpp $(REP_DIR)/src/lib/qt5/qgenodeviewwidget

LIBS += libc qoost qt5_core qt5_gui qt5_qpa_genode qt5_widgets

CC_CXX_WARN_STRICT =
