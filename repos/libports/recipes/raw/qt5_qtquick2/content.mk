content: qt5_qtquick2.tar

PORT_DIR := $(call port_dir,$(REP_DIR)/ports/qt5)

INSTALL_DIR := qt/qml/QtQuick.2
QMLDIR      := $(INSTALL_DIR)/qmldir

$(INSTALL_DIR):
	mkdir -p $@

$(QMLDIR): $(INSTALL_DIR)
	cp $(PORT_DIR)/src/lib/qt5/qt5/qtdeclarative/src/imports/qtquick2/qmldir $@

qt5_qtquick2.tar: $(QMLDIR)
	tar --owner=0 --group=0 --numeric-owner --mode='go=' --mtime='1970-01-01' -cf $@ qt
	rm -rf qt
