CMAKE_LISTS_DIR = $(PRG_DIR)

CMAKE_TARGET_BINARIES = test-qt_core_cmake

QT5_PORT_LIBS = libQt5Core

LIBS = libc libm qt5_component stdcxx $(QT5_PORT_LIBS)

include $(call select_from_repositories,lib/import/import-qt5_cmake.mk)
