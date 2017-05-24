include $(REP_DIR)/lib/import/import-qt5_qjpeg.mk

SRC_CC = main.cpp \
         moc_main.cpp \
         qjpeghandler.cpp \
         import_plugin.cc

INC_DIR += $(QT5_CONTRIB_DIR)/qtbase/src/plugins/imageformats/jpeg \
           $(QT5_CONTRIB_DIR)/qtbase/include/QtGui/$(QT_VERSION)/QtGui \
           $(QT5_CONTRIB_DIR)/qtbase/include/QtGui/$(QT_VERSION) \
           $(QT5_CONTRIB_DIR)/qtbase/include/QtCore/$(QT_VERSION)/QtCore \
           $(QT5_CONTRIB_DIR)/qtbase/include/QtCore/$(QT_VERSION)

LIBS += qt5_gui qt5_core jpeg

vpath % $(REP_DIR)/src/lib/qt5/qtbase/src/plugins/imageformats/jpeg
vpath % $(QT5_CONTRIB_DIR)/qtbase/src/plugins/imageformats/jpeg
