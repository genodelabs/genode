include $(REP_DIR)/lib/import/import-qt_gui.mk

SHARED_LIB = yes

# use default warning level to avoid noise when compiling contrib code
CC_WARN = -Wno-unused-but-set-variable -Wno-deprecated-declarations

include $(REP_DIR)/lib/mk/qt_gui_generated.inc

QT_DEFINES += -DQT_NO_QWS_SIGNALHANDLER

# add Genode-specific sources
QT_SOURCES += qkbdpc101_qws.cpp \
              qwindowsurface_nitpicker_qws.cpp \
              moc_qwindowsurface_nitpicker_qws_p.cpp \
              qscreennitpicker_qws.cpp \
              qmousenitpicker_qws.cpp \
              qkbdnitpicker_qws.cpp \
              qinputnitpicker_qws.cpp \
              moc_qinputnitpicker_qws.cpp

# remove unneeded files to prevent moc warnings
COMPILER_MOC_HEADER_MAKE_ALL_FILES_FILTER_OUT = \
  moc_qsoundqss_qws.cpp \
  moc_qcopchannel_qws.cpp \
  moc_qtransportauth_qws.cpp \
  moc_qtransportauth_qws_p.cpp \
  moc_qwssocket_qws.cpp \
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
  qsoundqss_qws.moc \
  qcopchannel_qws.moc \
  qsound_qws.moc \
  qprintpreviewwidget.moc \
  qprintdialog_unix.moc \
  qprintpreviewdialog.moc

# UI headers
qfiledialog.o: ui_qfiledialog.h

include $(REP_DIR)/lib/mk/qt.inc

INC_DIR += $(REP_DIR)/include/qt4/QtGui/private \
           $(REP_DIR)/contrib/$(QT4)/include/QtGui/private

LIBS += qt_core libpng zlib libc libm freetype jpeg

vpath % $(REP_DIR)/include/qt4/QtGui
vpath % $(REP_DIR)/include/qt4/QtGui/private
