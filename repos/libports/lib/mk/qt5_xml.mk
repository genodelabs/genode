include $(call select_from_repositories,lib/import/import-qt5_xml.mk)

SHARED_LIB = yes

include $(REP_DIR)/lib/mk/qt5_xml_generated.inc

include $(REP_DIR)/lib/mk/qt5.inc

LIBS += qt5_core

CC_CXX_WARN_STRICT =
