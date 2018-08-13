include $(call select_from_repositories,lib/import/import-qt5_test.mk)

SHARED_LIB = yes

include $(REP_DIR)/lib/mk/qt5_test_generated.inc

include $(REP_DIR)/lib/mk/qt5.inc

LIBS += qt5_core

CC_CXX_WARN_STRICT =
