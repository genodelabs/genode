IMPORT_QT5_INC=$(call select_from_repositories,lib/import/import-qt5.inc)

include $(IMPORT_QT5_INC)

SHARED_LIB = yes

include $(REP_DIR)/lib/mk/qt5_qtvirtualkeyboardplugin_generated.inc

QT_DEFINES += -UQT_STATICPLUGIN

include $(REP_DIR)/lib/mk/qt5.inc

LIBS += qt5_core qt5_gui qt5_qml qt5_quick qt5_svg

# install the Qt plugin

QT_PLUGIN_NAME        := qt5_qtvirtualkeyboardplugin
QT_PLUGIN_INSTALL_DIR := qt/plugins/platforminputcontexts
QT_PLUGIN             := $(QT_PLUGIN_INSTALL_DIR)/$(QT_PLUGIN_NAME).lib.so
TAR_ARCHIVE           := $(BUILD_BASE_DIR)/bin/$(QT_PLUGIN_NAME).tar

vpath % $(QT5_CONTRIB_DIR)/qtvirtualkeyboard/src/virtualkeyboard/content
vpath % $(QT5_CONTRIB_DIR)/qtvirtualkeyboard/src/virtualkeyboard/content/styles/retro
vpath % $(QT5_CONTRIB_DIR)/qtvirtualkeyboard/src/virtualkeyboard/content/styles/default

$(QTQUICK_INSTALL_DIR):
	$(VERBOSE)mkdir -p $@

$(QT_PLUGIN_INSTALL_DIR):
	$(VERBOSE)mkdir -p $@

$(QT_PLUGIN): $(QT_PLUGIN_INSTALL_DIR) $(QT_PLUGIN_NAME).lib.so.stripped
	$(VERBOSE)cp $(QT_PLUGIN_NAME).lib.so.stripped $@

$(TAR_ARCHIVE): $(QT_PLUGIN)
	$(VERBOSE)tar chf $@ qt

$(QT_PLUGIN_NAME).lib.tag: $(TAR_ARCHIVE)

#
# unfortunately, these clean rules don't trigger
#

clean-tar_archive:
	rm -rf $(TAR_ARCHIVE)

clean: clean-tar_archive

CC_CXX_WARN_STRICT =
