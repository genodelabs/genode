include $(call select_from_repositories,lib/import/import-qt5_qpa_genode.mk)

SHARED_LIB = yes

SRC_CC = qgenericunixeventdispatcher.cpp \
         qunixeventdispatcher.cpp \
         qfontengine_ft.cpp \
         qfreetypefontdatabase.cpp \
         qeglconvenience.cpp \
         moc_qunixeventdispatcher_qpa_p.cpp

SRC_CC += main.cpp \
          qgenodeclipboard.cpp \
          qgenodecursor.cpp \
          qgenodeglcontext.cpp \
          qgenodeintegration.cpp \
          qgenodeplatformwindow.cpp \
          qgenodewindowsurface.cpp \
          moc_qgenodeclipboard.cpp \
          moc_qgenodeplatformwindow.cpp \
          moc_qgenodewindowsurface.cpp \
          moc_qgenodeintegrationplugin.cpp

ifeq ($(CONTRIB_DIR),)

INC_DIR += $(call select_from_repositories,include/QtEglSupport/$(QT_VERSION)) \
           $(call select_from_repositories,include/QtEglSupport/$(QT_VERSION)/QtEglSupport/private) \
           $(call select_from_repositories,include/QtEventDispatcherSupport/$(QT_VERSION)) \
           $(call select_from_repositories,include/QtEventDispatcherSupport/$(QT_VERSION)/QtEventDispatcherSupport/private) \
           $(call select_from_repositories,include/QtFontDatabaseSupport/$(QT_VERSION)) \
           $(call select_from_repositories,include/QtFontDatabaseSupport/$(QT_VERSION)/QtFontDatabaseSupport/private) \
           $(call select_from_repositories,include/QtInputSupport/$(QT_VERSION)/QtInputSupport/private)

vpath qunixeventdispatcher_qpa_p.h $(call select_from_repositories,include/QtEventDispatcherSupport/$(QT_VERSION)/QtEventDispatcherSupport/private)
vpath qgenodeplatformwindow.h   $(call select_from_repositories,include/qt5/qpa_genode)

else

INC_DIR += $(QT5_PORT_DIR)/include/QtEglSupport/$(QT_VERSION) \
           $(QT5_PORT_DIR)/include/QtEventDispatcherSupport/$(QT_VERSION) \
           $(QT5_PORT_DIR)/include/QtFontDatabaseSupport/$(QT_VERSION) \
           $(QT5_PORT_DIR)/include/QtInputSupport/$(QT_VERSION)

vpath %.h  $(REP_DIR)/include/qt5/qpa_genode

endif

LIBS += qt5_gui qt5_core qoost egl freetype libc

vpath % $(QT5_CONTRIB_DIR)/qtbase/src/platformsupport/eglconvenience
vpath % $(QT5_CONTRIB_DIR)/qtbase/src/platformsupport/eventdispatchers
vpath % $(QT5_CONTRIB_DIR)/qtbase/src/platformsupport/fontdatabases/freetype
vpath % $(REP_DIR)/src/lib/qt5/qtbase/src/plugins/platforms/genode

CC_CXX_WARN_STRICT =
