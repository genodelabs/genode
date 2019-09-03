content: qt5_qtquick_virtualkeyboard.tar

PORT_DIR := $(call port_dir,$(REP_DIR)/ports/qt5)

INSTALL_DIR     := qt/qml/QtQuick/VirtualKeyboard
QMLDIR          := $(INSTALL_DIR)/qmldir
SETTINGS_QMLDIR := $(INSTALL_DIR)/Settings/qmldir
STYLES_QMLDIR   := $(INSTALL_DIR)/Styles/qmldir

$(INSTALL_DIR):
	mkdir -p $@

$(INSTALL_DIR)/Settings:
	mkdir -p $@

$(INSTALL_DIR)/Styles:
	mkdir -p $@

$(QMLDIR): $(INSTALL_DIR)
	cp $(PORT_DIR)/src/lib/qt5/qt5/qtvirtualkeyboard/src/import/qmldir $@

$(SETTINGS_QMLDIR): $(INSTALL_DIR)/Settings
	cp $(PORT_DIR)/src/lib/qt5/qt5/qtvirtualkeyboard/src/settings/qmldir $@

$(STYLES_QMLDIR): $(INSTALL_DIR)/Styles
	cp $(PORT_DIR)/src/lib/qt5/qt5/qtvirtualkeyboard/src/styles/qmldir $@

qt5_qtquick_virtualkeyboard.tar: $(QMLDIR) $(SETTINGS_QMLDIR) $(STYLES_QMLDIR)
	tar --owner=0 --group=0 --numeric-owner --mode='go=' --mtime='1970-01-01 00:00+00' -cf $@ qt
	rm -rf qt
