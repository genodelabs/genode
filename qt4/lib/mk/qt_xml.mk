include $(REP_DIR)/lib/import/import-qt_xml.mk

SHARED_LIB = yes

# extracted from src/xml/Makefile
QT_DEFINES += -DQT_BUILD_XML_LIB -DQT_NO_USING_NAMESPACE -DQT_NO_CAST_TO_ASCII -DQT_ASCII_CAST_WARNINGS -DQT_MOC_COMPAT -DQT_NO_DEBUG -DQT_CORE_LIB

include $(REP_DIR)/lib/mk/qt_xml_generated.inc

INC_DIR += $(REP_DIR)/include/qt4/QtXml/private \
           $(REP_DIR)/contrib/$(QT4)/include/QtXml/private \

LIBS += qt_core libc

vpath % $(REP_DIR)/include/qt4/QtXml
vpath % $(REP_DIR)/include/qt4/QtXml/private

vpath % $(REP_DIR)/src/lib/qt4/src/xml/dom
vpath % $(REP_DIR)/src/lib/qt4/src/xml/sax

vpath % $(REP_DIR)/contrib/$(QT4)/src/xml/dom
vpath % $(REP_DIR)/contrib/$(QT4)/src/xml/sax

include $(REP_DIR)/lib/mk/qt.inc
