QMAKE_PROJECT_FILE = $(PRG_DIR)/mixer_gui_qt.pro

QMAKE_TARGET_BINARIES = mixer_gui_qt

QT5_PORT_LIBS = libQt5Core libQt5Gui libQt5Widgets

LIBS = qt5_qmake base libc libm mesa stdcxx qoost

QT5_COMPONENT_LIB_SO =

QT5_GENODE_LIBS_APP += ld.lib.so

qmake_prepared.tag: $(addprefix build_dependencies/lib/,$(QT5_GENODE_LIBS_APP))
