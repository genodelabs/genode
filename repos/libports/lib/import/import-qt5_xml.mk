IMPORT_QT5_INC=$(call select_from_repositories,lib/import/import-qt5.inc)

include $(IMPORT_QT5_INC)

ifeq ($(CONTRIB_DIR),)
QT5_XML_INC_DIR := $(realpath $(call select_from_repositories,include/QtXml)/..)
else
QT5_XML_INC_DIR := $(QT5_PORT_DIR)/include
endif

QT5_INC_DIR += $(QT5_XML_INC_DIR)
QT5_INC_DIR += $(QT5_XML_INC_DIR)/QtXml
QT5_INC_DIR += $(QT5_XML_INC_DIR)/QtXml/$(QT_VERSION)
QT5_INC_DIR += $(QT5_XML_INC_DIR)/QtXml/$(QT_VERSION)/QtXml
QT5_INC_DIR += $(QT5_XML_INC_DIR)/QtXml/$(QT_VERSION)/QtXml/private
