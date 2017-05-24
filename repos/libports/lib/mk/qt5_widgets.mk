include $(REP_DIR)/lib/import/import-qt5_widgets.mk

SHARED_LIB = yes

include $(REP_DIR)/lib/mk/qt5_widgets_generated.inc

# UI headers
qfiledialog.o: ui_qfiledialog.h

include $(REP_DIR)/lib/mk/qt5.inc

INC_DIR += $(QT5_CONTRIB_DIR)/qtbase/include/QtWidgets/$(QT_VERSION)/QtWidgets \
           $(QT5_CONTRIB_DIR)/qtbase/include/QtGui/$(QT_VERSION) \
           $(QT5_CONTRIB_DIR)/qtbase/include/QtGui/$(QT_VERSION)/QtGui \
           $(QT5_CONTRIB_DIR)/qtbase/include/QtCore/$(QT_VERSION) \
           $(QT5_CONTRIB_DIR)/qtbase/include/QtCore/$(QT_VERSION)/QtCore

LIBS += qt5_gui
