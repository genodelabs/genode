IMPORT_QT5_INC=$(call select_from_repositories,lib/import/import-qt5.inc)

include $(IMPORT_QT5_INC)

SHARED_LIB = yes

include $(REP_DIR)/lib/mk/qt5_qtquickvirtualkeyboardsettingsplugin_generated.inc

QT_DEFINES += -UQT_STATICPLUGIN

include $(REP_DIR)/lib/mk/qt5.inc

LIBS += qt5_qml qt5_virtualkeyboard

# install the QML plugin

QML_PLUGIN_NAME := qt5_qtquickvirtualkeyboardsettingsplugin
QML_INSTALL_DIR := qt/qml/QtQuick/VirtualKeyboard/Settings

QML_PLUGIN      := $(QML_INSTALL_DIR)/$(QML_PLUGIN_NAME).lib.so
TAR_ARCHIVE     := $(BUILD_BASE_DIR)/bin/$(QML_PLUGIN_NAME).tar

$(QML_INSTALL_DIR):
	$(VERBOSE)mkdir -p $@

$(QML_PLUGIN): $(QML_INSTALL_DIR) $(QML_PLUGIN_NAME).lib.so.stripped
	$(VERBOSE)cp $(QML_PLUGIN_NAME).lib.so.stripped $@

$(TAR_ARCHIVE): $(QML_PLUGIN)
	$(VERBOSE)tar cf $@ qt

ifneq ($(call select_from_ports,qt5),)
$(QML_PLUGIN_NAME).lib.tag: $(TAR_ARCHIVE)
endif

#
# unfortunately, these clean rules don't trigger
#

clean-tar_archive:
	rm -rf $(TAR_ARCHIVE)

clean: clean-tar_archive

CC_CXX_WARN_STRICT =
