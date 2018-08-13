IMPORT_QT5_INC=$(call select_from_repositories,lib/import/import-qt5.inc)

include $(IMPORT_QT5_INC)

ifeq ($(CONTRIB_DIR),)
QT5_TEST_INC_DIR := $(realpath $(call select_from_repositories,include/QtTest)/..)
else
QT5_TEST_INC_DIR := $(QT5_PORT_DIR)/include
endif

QT5_INC_DIR += $(QT5_TEST_INC_DIR)
QT5_INC_DIR += $(QT5_TEST_INC_DIR)/QtTest
QT5_INC_DIR += $(QT5_TEST_INC_DIR)/QtTest/$(QT_VERSION)
QT5_INC_DIR += $(QT5_TEST_INC_DIR)/QtTest/$(QT_VERSION)/QtTest
QT5_INC_DIR += $(QT5_TEST_INC_DIR)/QtTest/$(QT_VERSION)/QtTest/private
