QMAKE_PROJECT_FILE = $(QT_DIR)/qtbase/examples/widgets/widgets/tooltips/tooltips.pro

QMAKE_TARGET_BINARIES = tooltips

QT5_PORT_LIBS = libQt5Core libQt5Gui libQt5PrintSupport libQt5Widgets

LIBS = qt5_qmake libc libm mesa qt5_component stdcxx
