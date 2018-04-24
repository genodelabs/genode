IMPORT_QT5_INC=$(call select_from_repositories,lib/import/import-qt5.inc)

include $(IMPORT_QT5_INC)

ifeq ($(CONTRIB_DIR),)
QT5_SCRIPTCLASSIC_INC_DIR := $(realpath $(call select_from_repositories,include/QtScript)/..)
else
QT5_SCRIPTCLASSIC_INC_DIR := $(QT5_PORT_DIR)/include
endif

QT5_INC_DIR += $(QT5_SCRIPTCLASSIC_INC_DIR)
QT5_INC_DIR += $(QT5_SCRIPTCLASSIC_INC_DIR)/QtScript

QT_DEFINES += -DQ_SCRIPT_EXPORT=
