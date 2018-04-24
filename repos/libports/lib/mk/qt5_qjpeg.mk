include $(call select_from_repositories,lib/import/import-qt5_qjpeg.mk)

SHARED_LIB = yes

SRC_CC = main.cpp \
         moc_main.cpp \
         qjpeghandler.cpp \
         import_plugin.cc

INC_DIR += $(QT5_CONTRIB_DIR)/qtbase/src/plugins/imageformats/jpeg

LIBS += qt5_gui qt5_core libc jpeg

vpath % $(REP_DIR)/src/lib/qt5/qtbase/src/plugins/imageformats/jpeg
vpath % $(QT5_CONTRIB_DIR)/qtbase/src/plugins/imageformats/jpeg

CC_CXX_WARN_STRICT =
