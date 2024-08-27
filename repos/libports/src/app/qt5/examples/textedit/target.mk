QMAKE_PROJECT_FILE = $(QT_DIR)/qtbase/examples/widgets/richtext/textedit/textedit.pro

QMAKE_TARGET_BINARIES = textedit

QT5_PORT_LIBS = libQt5Core libQt5Gui libQt5PrintSupport libQt5Widgets

LIBS = qt5_qmake libc libm mesa qt5_component stdcxx
