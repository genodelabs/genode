include $(REP_DIR)/lib/import/import-qt5_xml.mk

SHARED_LIB = yes

include $(REP_DIR)/lib/mk/qt5_xml_generated.inc

include $(REP_DIR)/lib/mk/qt5.inc

INC_DIR += $(QT5_CONTRIB_DIR)/qtbase/include/QtCore/$(QT_VERSION)/QtCore \
