QMAKE_PROJECT_FILE = $(PRG_DIR)/qt_quick.pro

QMAKE_TARGET_BINARIES = test-qt_quick

QT6_PORT_LIBS += libQt6Core libQt6Gui libQt6OpenGL libQt6Network
QT6_PORT_LIBS += libQt6Qml libQt6QmlModels libQt6Quick

LIBS = qt6_qmake libc libm mesa qt6_component stdcxx
