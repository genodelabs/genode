ifeq ($(CONTRIB_DIR),)
QT6_TOOLS_DIR       = $(call select_from_repositories,src/lib/qt6_tools)
else
QT6_TOOLS_PORT_DIR := $(call select_from_ports,qt6_tools)
QT6_TOOLS_DIR       = $(QT6_TOOLS_PORT_DIR)/src/lib/qt6_tools
endif

QMAKE_PROJECT_FILE = $(QT6_TOOLS_DIR)/examples/linguist/i18n/i18n.pro

QMAKE_TARGET_BINARIES = i18n

QT6_PORT_LIBS = libQt6Core \
                libQt6Gui \
                libQt6Widgets

LIBS = qt6_qmake libc libm qt6_component stdcxx mesa egl
