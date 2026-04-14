PORT_DIR := $(call port_dir,$(REP_DIR)/ports/qt6_api)

MIRROR_LIB_SYMBOLS := libQt6LabsAnimation \
                      libQt6LabsFolderListModel \
                      libQt6LabsPlatform \
                      libQt6LabsQmlModels \
                      libQt6LabsSettings \
                      libQt6LabsSharedImage \
                      libQt6LabsWavefrontMesh \
                      libQt6Qml \
                      libQt6QmlCompiler \
                      libQt6QmlCore \
                      libQt6QmlLocalStorage \
                      libQt6QmlMeta \
                      libQt6QmlModels \
                      libQt6QmlNetwork \
                      libQt6QmlWorkerScript \
                      libQt6QmlXmlListModel \
                      libQt6Quick \
                      libQt6QuickControls2 \
                      libQt6QuickControls2Basic \
                      libQt6QuickControls2BasicStyleImpl \
                      libQt6QuickControls2FluentWinUI3StyleImpl \
                      libQt6QuickControls2Fusion \
                      libQt6QuickControls2FusionStyleImpl \
                      libQt6QuickControls2Imagine \
                      libQt6QuickControls2ImagineStyleImpl \
                      libQt6QuickControls2Impl \
                      libQt6QuickControls2Material \
                      libQt6QuickControls2MaterialStyleImpl \
                      libQt6QuickControls2Universal \
                      libQt6QuickControls2UniversalStyleImpl \
                      libQt6QuickDialogs2 \
                      libQt6QuickDialogs2QuickImpl \
                      libQt6QuickDialogs2Utils \
                      libQt6QuickEffects \
                      libQt6QuickLayouts \
                      libQt6QuickParticles \
                      libQt6QuickShapes \
                      libQt6QuickTemplates2 \
                      libQt6QuickTest \
                      libQt6QuickVectorImage \
                      libQt6QuickWidgets

content: $(MIRROR_LIB_SYMBOLS)

$(MIRROR_LIB_SYMBOLS):
	mkdir -p lib/symbols
	cp $(PORT_DIR)/src/lib/qt6_api/lib/symbols/$@ lib/symbols/

content: LICENSE

LICENSE:
	cp $(PORT_DIR)/src/lib/qt6_api/LICENSE.LGPL3 $@
