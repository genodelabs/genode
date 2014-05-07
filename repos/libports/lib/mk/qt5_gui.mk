include $(REP_DIR)/lib/import/import-qt5_gui.mk

SHARED_LIB = yes

# use default warning level to avoid noise when compiling contrib code
CC_WARN = -Wno-unused-but-set-variable -Wno-deprecated-declarations

include $(REP_DIR)/lib/mk/qt5_gui_generated.inc

# remove unneeded files to prevent moc warnings
COMPILER_MOC_HEADER_MAKE_ALL_FILES_FILTER_OUT = \
  moc_qsessionmanager.cpp \
  moc_qsound.cpp \
  moc_qsound_p.cpp \
  moc_qmenudata.cpp \
  moc_qprintpreviewwidget.cpp \
  moc_qabstractprintdialog.cpp \
  moc_qabstractpagesetupdialog.cpp \
  moc_qpagesetupdialog.cpp \
  moc_qprintdialog.cpp \
  moc_qprintpreviewdialog.cpp \
  moc_qpagesetupdialog_unix_p.cpp

COMPILER_MOC_SOURCE_MAKE_ALL_FILES_FILTER_OUT = \
  qprintpreviewwidget.moc \
  qprintdialog_unix.moc \
  qprintpreviewdialog.moc

# UI headers
qfiledialog.o: ui_qfiledialog.h

include $(REP_DIR)/lib/mk/qt5.inc

INC_DIR += $(REP_DIR)/include/qt5/qtbase/QtGui/private \
           $(REP_DIR)/contrib/$(QT5)/qtbase/include/QtGui/$(QT_VERSION) \
           $(REP_DIR)/contrib/$(QT5)/qtbase/include/QtGui/$(QT_VERSION)/QtGui \
           $(REP_DIR)/contrib/$(QT5)/qtbase/include/QtGui/$(QT_VERSION)/QtGui/private \
           $(REP_DIR)/contrib/$(QT5)/qtbase/include/QtCore/$(QT_VERSION) \
           $(REP_DIR)/contrib/$(QT5)/qtbase/include/QtCore/$(QT_VERSION)/QtCore

LIBS += qt5_core jpeg zlib libpng gallium
