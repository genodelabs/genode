include $(REP_DIR)/lib/import/import-qt_xml.mk

SHARED_LIB = yes

include $(REP_DIR)/lib/mk/qt_xml_generated.inc

include $(REP_DIR)/lib/mk/qt.inc

INC_DIR += $(REP_DIR)/include/qt4/QtXml/private \
           $(REP_DIR)/contrib/$(QT4)/include/QtXml/private \

LIBS += qt_core libc

vpath % $(REP_DIR)/include/qt4/QtXml
vpath % $(REP_DIR)/include/qt4/QtXml/private
