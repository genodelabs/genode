IMPORT_QT5_INC=$(call select_from_repositories,lib/import/import-qt5.inc)

include $(IMPORT_QT5_INC)

ifeq ($(CONTRIB_DIR),)
QT5_QUICKCONTROLS2_INC_DIR := $(realpath $(call select_from_repositories,include/QtQuickControls2)/..)
else
QT5_QUICKCONTROLS2_INC_DIR := $(QT5_PORT_DIR)/include
endif

QT5_INC_DIR += $(QT5_QUICKCONTROLS2_INC_DIR)
QT5_INC_DIR += $(QT5_QUICKCONTROLS2_INC_DIR)/QtQuickControls2
QT5_INC_DIR += $(QT5_QUICKCONTROLS2_INC_DIR)/QtQuickControls2/$(QT_VERSION)
QT5_INC_DIR += $(QT5_QUICKCONTROLS2_INC_DIR)/QtQuickControls2/$(QT_VERSION)/QtQuickControls2
QT5_INC_DIR += $(QT5_QUICKCONTROLS2_INC_DIR)/QtQuickControls2/$(QT_VERSION)/QtQuickControls2/private
