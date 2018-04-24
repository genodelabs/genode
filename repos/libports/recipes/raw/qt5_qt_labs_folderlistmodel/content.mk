content: qt5_qt_labs_folderlistmodel.tar

PORT_DIR := $(call port_dir,$(REP_DIR)/ports/qt5)

INSTALL_DIR := qt/qml/Qt/labs/folderlistmodel
QMLDIR      := $(INSTALL_DIR)/qmldir

$(INSTALL_DIR):
	mkdir -p $@

$(QMLDIR): $(INSTALL_DIR)
	cp $(PORT_DIR)/src/lib/qt5/qt5/qtdeclarative/src/imports/folderlistmodel/qmldir $@

qt5_qt_labs_folderlistmodel.tar: $(QMLDIR)
	tar cf $@ qt
	rm -rf qt
