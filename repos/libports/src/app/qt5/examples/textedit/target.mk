QT5_PORT_DIR := $(call select_from_ports,qt5)
QT5_CONTRIB_DIR := $(QT5_PORT_DIR)/src/lib/qt5/qt5

QMAKE_PROJECT_PATH = $(QT5_CONTRIB_DIR)/qtbase/examples/widgets/richtext/textedit
QMAKE_PROJECT_FILE = $(QMAKE_PROJECT_PATH)/textedit.pro

vpath % $(QMAKE_PROJECT_PATH)

include $(call select_from_repositories,src/app/qt5/tmpl/target_defaults.inc)

include $(call select_from_repositories,src/app/qt5/tmpl/target_final.inc)

LIBS += qt5_component qt5_printsupport

CC_CXX_WARN_STRICT =
