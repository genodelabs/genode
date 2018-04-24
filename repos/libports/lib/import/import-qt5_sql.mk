IMPORT_QT5_INC=$(call select_from_repositories,lib/import/import-qt5.inc)

include $(IMPORT_QT5_INC)

ifeq ($(CONTRIB_DIR),)
QT5_SQL_INC_DIR := $(realpath $(call select_from_repositories,include/QtSql)/..)
else
QT5_SQL_INC_DIR := $(QT5_PORT_DIR)/include
endif

QT5_INC_DIR += $(QT5_SQL_INC_DIR)
QT5_INC_DIR += $(QT5_SQL_INC_DIR)/QtSql
QT5_INC_DIR += $(QT5_SQL_INC_DIR)/QtSql/$(QT_VERSION)
QT5_INC_DIR += $(QT5_SQL_INC_DIR)/QtSql/$(QT_VERSION)/QtSql
QT5_INC_DIR += $(QT5_SQL_INC_DIR)/QtSql/$(QT_VERSION)/QtSql/private
