include $(call select_from_repositories,lib/import/import-qt5_widgets.mk)

SHARED_LIB = yes

include $(REP_DIR)/lib/mk/qt5_widgets_generated.inc

# UI headers
qfiledialog.o: ui_qfiledialog.h

include $(REP_DIR)/lib/mk/qt5.inc

LIBS += qt5_gui qt5_core libc

CC_CXX_WARN_STRICT =
