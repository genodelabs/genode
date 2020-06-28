QMAKE_PROJECT_FILE = $(REP_DIR)/src/lib/qgenodeviewwidget/qgenodeviewwidget.pro

QMAKE_TARGET_BINARIES = libqgenodeviewwidget.lib.so

QT5_PORT_LIBS = libQt5Core libQt5Gui libQt5Widgets

LIBS = libc libm mesa qoost stdcxx $(QT5_PORT_LIBS)

include $(call select_from_repositories,lib/import/import-qt5_qmake.mk)

qmake_root/include/qgenodeviewwidget: qmake_root/include
	ln -snf $(call select_from_repositories,include/qgenodeviewwidget) $@

qmake_prepared.tag: qmake_root/include/qgenodeviewwidget

ifeq ($(called_from_lib_mk),yes)
all: build_with_qmake
endif
