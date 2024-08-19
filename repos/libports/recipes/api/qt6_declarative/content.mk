PORT_DIR := $(call port_dir,$(REP_DIR)/ports/qt6)

MIRROR_LIB_SYMBOLS := libQt6LabsAnimation \
                      libQt6LabsFolderListModel \
                      libQt6LabsQmlModels \
                      libQt6LabsSettings \
                      libQt6LabsSharedImage \
                      libQt6LabsWavefrontMesh \
                      libQt6Qml \
                      libQt6QmlCompiler \
                      libQt6QmlCore \
                      libQt6QmlLocalStorage \
                      libQt6QmlModels \
                      libQt6QmlWorkerScript \
                      libQt6QmlXmlListModel \
                      libQt6Quick \
                      libQt6QuickControls2 \
                      libQt6QuickControls2Impl \
                      libQt6QuickDialogs2 \
                      libQt6QuickDialogs2QuickImpl \
                      libQt6QuickDialogs2Utils \
                      libQt6QuickEffects \
                      libQt6QuickLayouts \
                      libQt6QuickParticles \
                      libQt6QuickShapes \
                      libQt6QuickTemplates2 \
                      libQt6QuickTest \
                      libQt6QuickWidgets

content: $(MIRROR_LIB_SYMBOLS)

$(MIRROR_LIB_SYMBOLS):
	mkdir -p lib/symbols
	cp $(PORT_DIR)/src/lib/qt6/genode/api/lib/symbols/$@ lib/symbols/

content: LICENSE

LICENSE:
	cp $(PORT_DIR)/src/lib/qt6/LICENSE.LGPL3 $@
