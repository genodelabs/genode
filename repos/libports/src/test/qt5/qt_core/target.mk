QMAKE_PROJECT_FILE = $(PRG_DIR)/qt_core.pro

QMAKE_TARGET_BINARIES = test-qt_core

QT5_PORT_LIBS = libQt5Core

LIBS = libc libm qt5_component stdcxx $(QT5_PORT_LIBS)

include $(call select_from_repositories,lib/import/import-qt5_qmake.mk)
