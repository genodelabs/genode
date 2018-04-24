IMPORT_QT5_INC=$(call select_from_repositories,lib/import/import-qt5.inc)

include $(IMPORT_QT5_INC)

ifeq ($(CONTRIB_DIR),)
QT5_DESIGNER_INC_DIR := $(realpath $(call select_from_repositories,include/QtDesigner)/..)
else
QT5_DESIGNER_INC_DIR := $(QT5_PORT_DIR)/include
endif

QT5_INC_DIR += $(QT5_DESIGNER_INC_DIR)
QT5_INC_DIR += $(QT5_DESIGNER_INC_DIR)/QtDesigner
QT5_INC_DIR += $(QT5_DESIGNER_INC_DIR)/QtDesigner/$(QT_VERSION)
QT5_INC_DIR += $(QT5_DESIGNER_INC_DIR)/QtDesigner/$(QT_VERSION)/QtDesigner
QT5_INC_DIR += $(QT5_DESIGNER_INC_DIR)/QtDesigner/$(QT_VERSION)/QtDesigner/private

ifeq ($(CONTRIB_DIR),)
QT5_UI_TOOLS_INC_DIR := $(realpath $(call select_from_repositories,include/QtUiTools)/..)
else
QT5_UI_TOOLS_INC_DIR := $(QT5_PORT_DIR)/include
endif

QT5_INC_DIR += $(QT5_UI_TOOLS_INC_DIR)
QT5_INC_DIR += $(QT5_UI_TOOLS_INC_DIR)/QtUiTools
QT5_INC_DIR += $(QT5_UI_TOOLS_INC_DIR)/QtUiTools/$(QT_VERSION)
QT5_INC_DIR += $(QT5_UI_TOOLS_INC_DIR)/QtUiTools/$(QT_VERSION)/QtUiTools
QT5_INC_DIR += $(QT5_UI_TOOLS_INC_DIR)/QtUiTools/$(QT_VERSION)/QtUiTools/private
