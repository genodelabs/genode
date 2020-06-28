QMAKE_PROJECT_FILE = $(PRG_DIR)/qt_launchpad.pro

QMAKE_TARGET_BINARIES = qt_launchpad

QT5_PORT_LIBS += libQt5Core libQt5Gui libQt5Widgets

LIBS = base libc libm mesa stdcxx launchpad $(QT5_PORT_LIBS)

include $(call select_from_repositories,lib/import/import-qt5_qmake.mk)

QT5_GENODE_LIBS_APP += ld.lib.so launchpad.lib.a
QT5_GENODE_LIBS_APP := $(filter-out qt5_component.lib.so,$(QT5_GENODE_LIBS_APP))

qmake_prepared.tag: qmake_root/lib/ld.lib.so qmake_root/lib/launchpad.lib.a
