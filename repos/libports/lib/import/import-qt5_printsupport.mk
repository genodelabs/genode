IMPORT_QT5_INC=$(call select_from_repositories,lib/import/import-qt5.inc)

include $(IMPORT_QT5_INC)

ifeq ($(CONTRIB_DIR),)
QT5_PRINTSUPPORT_INC_DIR := $(realpath $(call select_from_repositories,include/QtPrintSupport)/..)
else
QT5_PRINTSUPPORT_INC_DIR := $(QT5_PORT_DIR)/include
endif

QT5_INC_DIR += $(QT5_PRINTSUPPORT_INC_DIR)
QT5_INC_DIR += $(QT5_PRINTSUPPORT_INC_DIR)/QtPrintSupport
QT5_INC_DIR += $(QT5_PRINTSUPPORT_INC_DIR)/QtPrintSupport/$(QT_VERSION)
QT5_INC_DIR += $(QT5_PRINTSUPPORT_INC_DIR)/QtPrintSupport/$(QT_VERSION)/QtPrintSupport
QT5_INC_DIR += $(QT5_PRINTSUPPORT_INC_DIR)/QtPrintSupport/$(QT_VERSION)/QtPrintSupport/private

CC_CXX_OPT += -DQT_PRINTSUPPORT_LIB
