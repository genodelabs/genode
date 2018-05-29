content: qt5_qtquick_virtualkeyboard.tar

PORT_DIR := $(call port_dir,$(REP_DIR)/ports/qt5)

INSTALL_DIR   := qt/qml/QtQuick/VirtualKeyboard
QMLDIR        := $(INSTALL_DIR)/qmldir
STYLES_QMLDIR := $(INSTALL_DIR)/Styles/qmldir

$(INSTALL_DIR):
	mkdir -p $@

$(INSTALL_DIR)/Styles:
	mkdir -p $@

$(QMLDIR): $(INSTALL_DIR)
	cp $(PORT_DIR)/src/lib/qt5/qt5/qtvirtualkeyboard/src/virtualkeyboard/import/qmldir $@

$(STYLES_QMLDIR): $(INSTALL_DIR)/Styles
	cp $(PORT_DIR)/src/lib/qt5/qt5/qtvirtualkeyboard/src/virtualkeyboard/styles/qmldir $@

qt5_qtquick_virtualkeyboard.tar: $(QMLDIR) $(STYLES_QMLDIR)
	tar cf $@ --mtime='1970-01-01' qt
	rm -rf qt
