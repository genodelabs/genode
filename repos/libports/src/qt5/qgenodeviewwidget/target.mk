QMAKE_PROJECT_FILE = $(REP_DIR)/src/lib/qgenodeviewwidget/qgenodeviewwidget.pro

QMAKE_TARGET_BINARIES = libqgenodeviewwidget.lib.so

QT5_PORT_LIBS = libQt5Core libQt5Gui libQt5Widgets

LIBS = qt5_qmake libc libm mesa qoost stdcxx

build_dependencies/include/qgenodeviewwidget: build_dependencies/include
	ln -snf $(call select_from_repositories,include/qgenodeviewwidget) $@

qmake_prepared.tag: build_dependencies/include/qgenodeviewwidget
