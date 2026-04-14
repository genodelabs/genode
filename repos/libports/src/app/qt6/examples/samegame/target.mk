QMAKE_PROJECT_FILE = $(PRG_DIR)/samegame.pro

QMAKE_TARGET_BINARIES = samegame

QT6_PORT_LIBS += libQt6Core libQt6Gui libQt6OpenGL libQt6Network
QT6_PORT_LIBS += libQt6Qml libQt6QmlMeta libQt6QmlModels libQt6QmlWorkerScript libQt6Quick

LIBS = qt6_qmake libc libm mesa qt6_component stdcxx
