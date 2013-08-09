# identify the qt5 repository by searching for a file that is unique for qt5
QT5_REP_DIR := $(call select_from_repositories,lib/import/import-qt5.inc)
QT5_REP_DIR := $(realpath $(dir $(QT5_REP_DIR))../..)

include $(QT5_REP_DIR)/lib/mk/qt5_version.inc

QMAKE_PROJECT_PATH = $(realpath $(QT5_REP_DIR)/contrib/$(QT5)/qtscript/examples/script/qstetrix)
QMAKE_PROJECT_FILE = $(QMAKE_PROJECT_PATH)/qstetrix.pro

vpath % $(QMAKE_PROJECT_PATH)

include $(QT5_REP_DIR)/src/app/qt5/tmpl/target_defaults.inc

CC_CXX_OPT += -DQT_NO_SCRIPTTOOLS

include $(QT5_REP_DIR)/src/app/qt5/tmpl/target_final.inc
