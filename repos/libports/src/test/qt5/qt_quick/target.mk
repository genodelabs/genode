QMAKE_PROJECT_FILE = $(PRG_DIR)/qt_quick.pro

QMAKE_TARGET_BINARIES = test-qt_quick

QT5_PORT_LIBS += libQt5Core libQt5Gui libQt5Network
QT5_PORT_LIBS += libQt5Qml libQt5QmlModels libQt5Quick

LIBS = qt5_qmake libc libm mesa qt5_component stdcxx
