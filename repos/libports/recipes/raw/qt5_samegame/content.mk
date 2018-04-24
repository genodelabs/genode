content: qt5_samegame.tar

PORT_DIR := $(call port_dir,$(REP_DIR)/ports/qt5)

SAMEGAME3_RESOURCES := samegame.qml \
                       Dialog.qml \
                       Button.qml \
                       Block.qml \
                       samegame.js

SAMEGAME_RESOURCES := background.jpg \
                      blueStone.png \
                      greenStone.png \
                      redStone.png \
                      yellowStone.png

samegame:
	mkdir -p $@

samegame/shared/pics:
	mkdir -p $@

$(addprefix samegame/, $(SAMEGAME3_RESOURCES)): samegame
	cp $(PORT_DIR)/src/lib/qt5/qt5/qtdeclarative/examples/quick/tutorials/samegame/samegame3/$(notdir $@) $@

$(addprefix samegame/shared/pics/, $(SAMEGAME_RESOURCES)): samegame/shared/pics
	cp $(PORT_DIR)/src/lib/qt5/qt5/qtdeclarative/examples/quick/tutorials/samegame/shared/pics/$(notdir $@) $@


qt5_samegame.tar: $(addprefix samegame/, $(SAMEGAME3_RESOURCES)) \
                  $(addprefix samegame/shared/pics/, $(SAMEGAME_RESOURCES))
	tar cf $@ -C samegame .
	rm -rf samegame/
