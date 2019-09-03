include $(call select_from_repositories,lib/import/import-qt5_quicktemplates2.mk)

SHARED_LIB = yes

LIBS += qt5_core qt5_gui qt5_qml qt5_quick

include $(REP_DIR)/lib/mk/qt5_quicktemplates2_generated.inc

include $(REP_DIR)/lib/mk/qt5.inc

CC_CXX_WARN_STRICT =
