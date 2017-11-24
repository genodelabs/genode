include $(REP_DIR)/lib/import/import-qt5_qpa_nitpicker.mk

# get the correct harfbuzz header included
QT_DEFINES += -DQT_BUILD_GUI_LIB

SRC_CC = qgenericunixeventdispatcher.cpp \
         qunixeventdispatcher.cpp \
         qbasicfontdatabase.cpp \
         qfontengine_ft.cpp \
         qeglconvenience.cpp

SRC_CC += main.cpp \
          qgenodeclipboard.cpp \
          qnitpickercursor.cpp \
          qnitpickerglcontext.cpp \
          qnitpickerintegration.cpp \
          qnitpickerplatformwindow.cpp \
          qnitpickerwindowsurface.cpp \
          qsignalhandlerthread.cpp \
          moc_qnitpickerplatformwindow.cpp \
          moc_qnitpickerwindowsurface.cpp \
          moc_qnitpickerintegrationplugin.cpp \
          qevdevkeyboardhandler.cpp \
          moc_qunixeventdispatcher_qpa_p.cpp \
          moc_qevdevkeyboardhandler_p.cpp \
          moc_qsignalhandlerthread.cpp

INC_DIR += $(QT5_CONTRIB_DIR)/qtbase/src/platformsupport/eventdispatchers \
           $(QT5_CONTRIB_DIR)/qtbase/src/platformsupport/fontdatabases/basic \
           $(QT5_CONTRIB_DIR)/qtbase/src/3rdparty/harfbuzz/src \
           $(QT5_CONTRIB_DIR)/qtbase/include/QtEglSupport/$(QT_VERSION) \
           $(QT5_CONTRIB_DIR)/qtbase/include/QtGui/$(QT_VERSION) \
           $(QT5_CONTRIB_DIR)/qtbase/include/QtCore/$(QT_VERSION) \
           $(QT5_CONTRIB_DIR)/qtbase/include/QtCore/$(QT_VERSION)/QtCore

LIBS += qt5_xml qt5_gui qt5_core libm freetype mesa egl qoost

vpath % $(QT5_CONTRIB_DIR)/qtbase/src/platformsupport/eventdispatchers
vpath % $(QT5_CONTRIB_DIR)/qtbase/src/platformsupport/input/evdevkeyboard
vpath % $(QT5_CONTRIB_DIR)/qtbase/src/platformsupport/fontdatabases/basic
vpath % $(QT5_CONTRIB_DIR)/qtbase/src/platformsupport/eglconvenience
vpath % $(QT5_CONTRIB_DIR)/qtbase/src/gui/text
vpath % $(REP_DIR)/src/lib/qt5/qtbase/src/plugins/platforms/nitpicker
