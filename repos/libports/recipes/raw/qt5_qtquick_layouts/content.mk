content: qt5_qtquick_layouts.tar

PORT_DIR := $(call port_dir,$(REP_DIR)/ports/qt5)

INSTALL_DIR := qt/qml/QtQuick/Layouts
QMLDIR      := $(INSTALL_DIR)/qmldir

$(INSTALL_DIR):
	mkdir -p $@

$(QMLDIR): $(INSTALL_DIR)
	cp $(PORT_DIR)/src/lib/qt5/qt5/qtdeclarative/src/imports/layouts/qmldir $@

qt5_qtquick_layouts.tar: $(QMLDIR)
	tar cf $@ qt
	rm -rf qt
