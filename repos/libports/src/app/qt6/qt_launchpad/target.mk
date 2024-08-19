QMAKE_PROJECT_FILE = $(PRG_DIR)/qt_launchpad.pro

QMAKE_TARGET_BINARIES = qt_launchpad

QT6_PORT_LIBS += libQt6Core libQt6Gui libQt6Widgets

LIBS = qt6_qmake base libc libm mesa stdcxx launchpad

QT6_COMPONENT_LIB_SO =

QT6_GENODE_LIBS_APP += ld.lib.so launchpad.lib.a
qmake_prepared.tag: $(addprefix build_dependencies/lib/,$(QT6_GENODE_LIBS_APP))
