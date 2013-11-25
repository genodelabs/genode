include $(REP_DIR)/lib/import/import-qt5_qpa_nitpicker.mk

# get the correct harfbuzz header included
QT_DEFINES += -DQT_BUILD_GUI_LIB

SRC_CC = qgenericunixeventdispatcher.cpp \
         qunixeventdispatcher.cpp \
         qbasicfontdatabase.cpp \
         qfontengine_ft.cpp \
         qeglconvenience.cpp

SRC_CC += main.cpp \
          qnitpickerglcontext.cpp \
          qnitpickerintegration.cpp \
          qnitpickerwindowsurface.cpp \
          moc_qnitpickerplatformwindow.cpp \
          moc_qnitpickerwindowsurface.cpp \
          moc_qnitpickerintegrationplugin.cpp \
          qevdevkeyboardhandler.cpp \
          moc_qunixeventdispatcher_qpa_p.cpp \
          moc_qevdevkeyboardhandler_p.cpp

INC_DIR += $(REP_DIR)/contrib/$(QT5)/qtbase/src/platformsupport/eventdispatchers \
           $(REP_DIR)/contrib/$(QT5)/qtbase/src/platformsupport/input/evdevkeyboard \
           $(REP_DIR)/contrib/$(QT5)/qtbase/src/platformsupport/fontdatabases/basic \
           $(REP_DIR)/contrib/$(QT5)/qtbase/src/3rdparty/harfbuzz/src \
           $(REP_DIR)/src/lib/qt5/qtbase/src/plugins/platforms/nitpicker \
           $(REP_DIR)/contrib/$(QT5)/qtbase/include/QtPlatformSupport/$(QT_VERSION) \
           $(REP_DIR)/contrib/$(QT5)/qtbase/include/QtGui/$(QT_VERSION) \
           $(REP_DIR)/contrib/$(QT5)/qtbase/include/QtGui/$(QT_VERSION)/QtGui \
           $(REP_DIR)/contrib/$(QT5)/qtbase/include/QtCore/$(QT_VERSION) \
           $(REP_DIR)/contrib/$(QT5)/qtbase/include/QtCore/$(QT_VERSION)/QtCore

LIBS += qt5_xml qt5_gui qt5_core libm freetype gallium

vpath % $(call select_from_repositories,contrib/$(QT5)/qtbase/src/platformsupport/eventdispatchers)
vpath % $(call select_from_repositories,contrib/$(QT5)/qtbase/src/platformsupport/input/evdevkeyboard)
vpath % $(call select_from_repositories,contrib/$(QT5)/qtbase/src/platformsupport/fontdatabases/basic)
vpath % $(call select_from_repositories,contrib/$(QT5)/qtbase/src/platformsupport/eglconvenience)
vpath % $(call select_from_repositories,contrib/$(QT5)/qtbase/src/gui/text)
vpath % $(call select_from_repositories,src/lib/qt5/qtbase/src/plugins/platforms/nitpicker)

