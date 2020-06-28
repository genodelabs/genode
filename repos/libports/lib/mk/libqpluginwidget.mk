QMAKE_PROJECT_FILE = $(REP_DIR)/src/lib/qpluginwidget/qpluginwidget.pro

QMAKE_TARGET_BINARIES = libqpluginwidget.lib.so

QT5_PORT_LIBS = libQt5Core libQt5Gui libQt5Network libQt5Widgets libqgenode

LIBS = libc libm libqgenodeviewwidget mesa qoost stdcxx zlib $(QT5_PORT_LIBS)

include $(call select_from_repositories,lib/import/import-qt5_qmake.mk)

QT5_GENODE_LIBS_SHLIB += libqgenodeviewwidget.lib.so libqgenode.lib.so

qmake_root/include/qpluginwidget: qmake_root/include
	ln -snf $(call select_from_repositories,include/qpluginwidget) $@

qmake_prepared.tag: qmake_root/include/qpluginwidget \
                    qmake_root/lib/libqgenodeviewwidget.lib.so \
                    qmake_root/lib/libqgenode.lib.so

ifeq ($(called_from_lib_mk),yes)
all: build_with_qmake
endif
