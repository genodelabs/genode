QMAKE_PROJECT_FILE = $(QT_DIR)/qtbase/examples/opengl/openglwindow/openglwindow.pro

QMAKE_TARGET_BINARIES = openglwindow

QT6_PORT_LIBS = libQt6Core libQt6Gui libQt6OpenGL

LIBS = qt6_qmake libc libm mesa qt6_component stdcxx
