QMAKE_PROJECT_FILE = $(QT_DIR)/qtbase/examples/widgets/richtext/textedit/textedit.pro

QMAKE_TARGET_BINARIES = textedit

QT5_PORT_LIBS = libQt5Core libQt5Gui libQt5PrintSupport libQt5Widgets

LIBS = libc libm mesa qt5_component stdcxx $(QT5_PORT_LIBS)

include $(call select_from_repositories,lib/import/import-qt5_qmake.mk)
