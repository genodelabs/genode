IMPORT_QT5_INC=$(call select_from_repositories,lib/import/import-qt5.inc)

include $(IMPORT_QT5_INC)

ifeq ($(CONTRIB_DIR),)
QT5_QUICKTEMPLATES2_INC_DIR := $(realpath $(call select_from_repositories,include/QtQuickTemplates2)/..)
else
QT5_QUICKTEMPLATES2_INC_DIR := $(QT5_PORT_DIR)/include
endif

QT5_INC_DIR += $(QT5_QUICKTEMPLATES2_INC_DIR)
QT5_INC_DIR += $(QT5_QUICKTEMPLATES2_INC_DIR)/QtQuickTemplates2
QT5_INC_DIR += $(QT5_QUICKTEMPLATES2_INC_DIR)/QtQuickTemplates2/$(QT_VERSION)
QT5_INC_DIR += $(QT5_QUICKTEMPLATES2_INC_DIR)/QtQuickTemplates2/$(QT_VERSION)/QtQuickTemplates2
QT5_INC_DIR += $(QT5_QUICKTEMPLATES2_INC_DIR)/QtQuickTemplates2/$(QT_VERSION)/QtQuickTemplates2/private
