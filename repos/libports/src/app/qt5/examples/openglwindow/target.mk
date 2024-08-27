QMAKE_PROJECT_FILE = $(QT_DIR)/qtbase/examples/gui/openglwindow/openglwindow.pro

QMAKE_TARGET_BINARIES = openglwindow

QT5_PORT_LIBS = libQt5Core libQt5Gui

LIBS = qt5_qmake libc libm mesa qt5_component stdcxx
