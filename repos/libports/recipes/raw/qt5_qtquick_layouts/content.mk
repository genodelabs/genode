content: qt5_qtquick_layouts.tar

PORT_DIR := $(call port_dir,$(REP_DIR)/ports/qt5)

INSTALL_DIR := qt/qml/QtQuick/Layouts
QMLDIR      := $(INSTALL_DIR)/qmldir

$(INSTALL_DIR):
	mkdir -p $@

$(QMLDIR): $(INSTALL_DIR)
	cp $(PORT_DIR)/src/lib/qt5/qt5/qtdeclarative/src/imports/layouts/qmldir $@

qt5_qtquick_layouts.tar: $(QMLDIR)
	tar --owner=0 --group=0 --numeric-owner --mode='go=' --mtime='1970-01-01' -cf $@ qt
	rm -rf qt
