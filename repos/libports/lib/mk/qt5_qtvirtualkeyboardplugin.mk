include $(REP_DIR)/lib/import/import-qt5_qtvirtualkeyboardplugin.mk

SHARED_LIB = yes

include $(REP_DIR)/lib/mk/qt5_qtvirtualkeyboardplugin_generated.inc

QT_DEFINES += -UQT_STATICPLUGIN

include $(REP_DIR)/lib/mk/qt5.inc

LIBS += qt5_qml qt5_quick qt5_svg qt5_qtvirtualkeyboardstylesplugin qt5_qquicklayoutsplugin  qt5_qmlfolderlistmodelplugin mesa

# install the QtQuick QML plugin

QT_PLUGIN_INSTALL_DIR 	:= $(BUILD_BASE_DIR)/bin/qt5_fs/qt/plugins/platforminputcontexts
QT_PLUGIN_NAME 		:= qt5_qtvirtualkeyboardplugin.lib.so
QT_PLUGIN      		:= $(QT_PLUGIN_INSTALL_DIR)/$(QT_PLUGIN_NAME)
QTQUICK_INSTALL_DIR 	:= $(BUILD_BASE_DIR)/bin/qt5_fs/qt/qml/QtQuick/VirtualKeyboard
QTQUICK_QMLDIR      	:= $(QTQUICK_INSTALL_DIR)/qmldir

QT5_REP_DIR := $(call select_from_repositories,lib/import/import-qt5.inc)
QT5_REP_DIR := $(realpath $(dir $(QT5_REP_DIR))../..)
include $(QT5_REP_DIR)/lib/mk/qt5_version.inc

QT5_CONTRIB_DIR := $(call select_from_ports,qt5)/src/lib/qt5/$(QT5)

vpath % $(QT5_CONTRIB_DIR)/qtvirtualkeyboard/src/virtualkeyboard/content
vpath % $(QT5_CONTRIB_DIR)/qtvirtualkeyboard/src/virtualkeyboard/content/styles/retro
vpath % $(QT5_CONTRIB_DIR)/qtvirtualkeyboard/src/virtualkeyboard/content/styles/default

$(QTQUICK_INSTALL_DIR):
	$(VERBOSE)mkdir -p $@

$(STYLES_INSTALL_DIR):
	$(VERBOSE)mkdir -p $@

$(QTQUICK_QMLDIR): $(QTQUICK_INSTALL_DIR)
	$(VERBOSE)cp $(QT5_CONTRIB_DIR)/qtvirtualkeyboard/src/virtualkeyboard/import/qmldir $(QTQUICK_INSTALL_DIR)

$(QT_PLUGIN_INSTALL_DIR):
	$(VERBOSE)mkdir -p $@

$(QT_PLUGIN): $(QT_PLUGIN_INSTALL_DIR)
	$(VERBOSE)ln -sf $(BUILD_BASE_DIR)/bin/$(QT_PLUGIN_NAME) $(QT_PLUGIN_INSTALL_DIR)/$(QT_PLUGIN_NAME)

ifneq ($(call select_from_ports,qt5),)
all: $(QT_PLUGIN) $(QTQUICK_QMLDIR) $(QTQUICK_PLUGIN)
endif

#
# unfortunately, these clean rules don't trigger
#

clean-qtquick_install_dir:
	rm -rf $(QT_PLUGIN_INSTALL_DIR)

clean: clean-qtquick_install_dir
