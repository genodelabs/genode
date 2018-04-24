IMPORT_QT5_INC=$(call select_from_repositories,lib/import/import-qt5.inc)

include $(IMPORT_QT5_INC)

#
# Public QtGui headers include OpenGL headers
#
# We cannot just extend the 'LIBS' variable here because 'import-*.mk' are
# included (in 'base/mk/lib.mk') by iterating through the elements of the
# 'LIBS' variable. Hence, we need to manually import the mesa snippet.
#
include $(call select_from_repositories,lib/import/import-mesa_api.mk)

ifeq ($(CONTRIB_DIR),)
QT5_GUI_INC_DIR := $(realpath $(call select_from_repositories,include/QtGui)/..)
else
QT5_GUI_INC_DIR := $(QT5_PORT_DIR)/include
endif

QT5_INC_DIR += $(QT5_GUI_INC_DIR)
QT5_INC_DIR += $(QT5_GUI_INC_DIR)/QtGui
QT5_INC_DIR += $(QT5_GUI_INC_DIR)/QtGui/$(QT_VERSION)
QT5_INC_DIR += $(QT5_GUI_INC_DIR)/QtGui/$(QT_VERSION)/QtGui
QT5_INC_DIR += $(QT5_GUI_INC_DIR)/QtGui/$(QT_VERSION)/QtGui/private
