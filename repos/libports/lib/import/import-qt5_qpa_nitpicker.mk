IMPORT_QT5_INC=$(call select_from_repositories,lib/import/import-qt5.inc)

include $(IMPORT_QT5_INC)

# include dirs needed for 'qnitpickerplatformwindow.h', which is used by
# the qt5_qnitpickerviewwidget library

INC_DIR += $(REP_DIR)/src/lib/qt5/qtbase/src/plugins/platforms/nitpicker \
           $(REP_DIR)/contrib/$(QT5)/qtbase/src/platformsupport/input/evdevkeyboard \
           $(REP_DIR)/contrib/$(QT5)/qtbase/include/QtGui/$(QT_VERSION)/QtGui \

