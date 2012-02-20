include $(REP_DIR)/lib/import/import-qt_gui.mk

SHARED_LIB = yes

# extracted from src/gui/Makefile
QT_DEFINES += -DQT_BUILD_GUI_LIB -DQT_NO_USING_NAMESPACE -DQT_NO_CAST_TO_ASCII -DQT_ASCII_CAST_WARNINGS -DQT_MOC_COMPAT -DQT_USE_FAST_OPERATOR_PLUS -DQT_USE_FAST_CONCATENATION -DQT_NO_FONTCONFIG -DQT_NO_OPENTYPE -DQT_NO_STYLE_MAC -DQT_NO_STYLE_WINDOWSVISTA -DQT_NO_STYLE_WINDOWSXP -DQT_NO_STYLE_GTK -DQT_NO_STYLE_WINDOWSCE -DQT_NO_STYLE_WINDOWSMOBILE -DQT_NO_STYLE_S60 -DQ_INTERNAL_QAPP_SRC -DQT_NO_DEBUG -DQT_CORE_LIB
QT_DEFINES += -DQT_NO_QWS_SIGNALHANDLER

# use default warning level to avoid noise when compiling contrib code
CC_WARN = -Wno-unused-but-set-variable -Wno-deprecated-declarations

include $(REP_DIR)/lib/mk/qt_gui_generated.inc

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

INC_DIR += $(REP_DIR)/include/qt4/QtGui/private \
           $(REP_DIR)/contrib/$(QT4)/include/QtGui/private \
           $(REP_DIR)/contrib/$(QT4)/src/3rdparty/harfbuzz/src

LIBS += qt_core libpng zlib libc libm freetype jpeg

vpath % $(REP_DIR)/include/qt4/QtGui
vpath % $(REP_DIR)/include/qt4/QtGui/private

vpath % $(REP_DIR)/src/lib/qt4/src/gui/embedded
vpath % $(REP_DIR)/src/lib/qt4/src/gui/animation
vpath % $(REP_DIR)/src/lib/qt4/src/gui/effects
vpath % $(REP_DIR)/src/lib/qt4/src/gui/kernel
vpath % $(REP_DIR)/src/lib/qt4/src/gui/image
vpath % $(REP_DIR)/src/lib/qt4/src/gui/painting
vpath % $(REP_DIR)/src/lib/qt4/src/gui/text
vpath % $(REP_DIR)/src/lib/qt4/src/gui/styles
vpath % $(REP_DIR)/src/lib/qt4/src/gui/widgets
vpath % $(REP_DIR)/src/lib/qt4/src/gui/dialogs
vpath % $(REP_DIR)/src/lib/qt4/src/gui/accessible
vpath % $(REP_DIR)/src/lib/qt4/src/gui/itemviews
vpath % $(REP_DIR)/src/lib/qt4/src/gui/inputmethod
vpath % $(REP_DIR)/src/lib/qt4/src/gui/graphicsview
vpath % $(REP_DIR)/src/lib/qt4/src/gui/util
vpath % $(REP_DIR)/src/lib/qt4/src/gui/statemachine
vpath % $(REP_DIR)/src/lib/qt4/src/gui/math3d


vpath % $(REP_DIR)/contrib/$(QT4)/src/gui/embedded
vpath % $(REP_DIR)/contrib/$(QT4)/src/gui/animation
vpath % $(REP_DIR)/contrib/$(QT4)/src/gui/effects
vpath % $(REP_DIR)/contrib/$(QT4)/src/gui/kernel
vpath % $(REP_DIR)/contrib/$(QT4)/src/gui/image
vpath % $(REP_DIR)/contrib/$(QT4)/src/gui/painting
vpath % $(REP_DIR)/contrib/$(QT4)/src/gui/text
vpath % $(REP_DIR)/contrib/$(QT4)/src/gui/styles
vpath % $(REP_DIR)/contrib/$(QT4)/src/gui/widgets
vpath % $(REP_DIR)/contrib/$(QT4)/src/gui/dialogs
vpath % $(REP_DIR)/contrib/$(QT4)/src/gui/accessible
vpath % $(REP_DIR)/contrib/$(QT4)/src/gui/itemviews
vpath % $(REP_DIR)/contrib/$(QT4)/src/gui/inputmethod
vpath % $(REP_DIR)/contrib/$(QT4)/src/gui/graphicsview
vpath % $(REP_DIR)/contrib/$(QT4)/src/gui/util
vpath % $(REP_DIR)/contrib/$(QT4)/src/gui/statemachine
vpath % $(REP_DIR)/contrib/$(QT4)/src/gui/math3d

include $(REP_DIR)/lib/mk/qt.inc
