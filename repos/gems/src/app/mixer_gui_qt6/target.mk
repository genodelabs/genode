QMAKE_PROJECT_FILE = $(PRG_DIR)/mixer_gui_qt.pro

QMAKE_TARGET_BINARIES = mixer_gui_qt

QT6_PORT_LIBS = libQt6Core libQt6Gui libQt6Widgets

LIBS = qt6_qmake base libc libm mesa stdcxx qoost

QT6_COMPONENT_LIB_SO =

QT6_GENODE_LIBS_APP += ld.lib.so
qmake_prepared.tag: $(addprefix build_dependencies/lib/,$(QT6_GENODE_LIBS_APP))
