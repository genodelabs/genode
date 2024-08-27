QMAKE_PROJECT_FILE = $(PRG_DIR)/qt_launchpad.pro

QMAKE_TARGET_BINARIES = qt_launchpad

QT5_PORT_LIBS += libQt5Core libQt5Gui libQt5Widgets

LIBS = qt5_qmake base libc libm mesa stdcxx launchpad

QT5_COMPONENT_LIB_SO =

QT5_GENODE_LIBS_APP += ld.lib.so launchpad.lib.a

qmake_prepared.tag: $(addprefix build_dependencies/lib/,$(QT5_GENODE_LIBS_APP))
