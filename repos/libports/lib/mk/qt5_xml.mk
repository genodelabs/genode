include $(REP_DIR)/lib/import/import-qt5_xml.mk

SHARED_LIB = yes

include $(REP_DIR)/lib/mk/qt5_xml_generated.inc

include $(REP_DIR)/lib/mk/qt5.inc

INC_DIR += $(REP_DIR)/include/qt5/qtbase/QtXml/private \
           $(QT5_CONTRIB_DIR)/qtbase/include/QtCore/$(QT_VERSION)/QtCore \

LIBS += qt5_core libc
