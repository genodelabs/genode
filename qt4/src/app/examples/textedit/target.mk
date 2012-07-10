# identify the qt4 repository by searching for a file that is unique for qt4
QT4_REP_DIR := $(call select_from_repositories,lib/import/import-qt4.inc)
QT4_REP_DIR := $(realpath $(dir $(QT4_REP_DIR))../..)

include $(QT4_REP_DIR)/lib/mk/qt_version.inc

QMAKE_PROJECT_PATH = $(realpath $(QT4_REP_DIR)/contrib/$(QT4)/demos/textedit)
QMAKE_PROJECT_FILE = $(QMAKE_PROJECT_PATH)/textedit.pro

vpath % $(QMAKE_PROJECT_PATH)

include $(QT4_REP_DIR)/src/app/tmpl/target_defaults.inc

include $(QT4_REP_DIR)/src/app/tmpl/target_final.inc

LIBS += libc_fs
