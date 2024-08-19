content: qt6_samegame.tar

PORT_DIR := $(call port_dir,$(REP_DIR)/ports/qt6)

SAMEGAME3_RESOURCES := samegame.qml \
                       Dialog.qml \
                       Button.qml \
                       Block.qml \
                       samegame.js

SAMEGAME_RESOURCES := background.jpg \
                      blueStone.png \
                      greenStone.png \
                      redStone.png

samegame:
	mkdir -p $@

samegame/pics:
	mkdir -p $@

$(addprefix samegame/, $(SAMEGAME3_RESOURCES)): samegame
	cp $(PORT_DIR)/src/lib/qt6/qtdeclarative/examples/quick/tutorials/samegame/samegame3/$(notdir $@) $@

$(addprefix samegame/pics/, $(SAMEGAME_RESOURCES)): samegame/pics
	cp $(PORT_DIR)/src/lib/qt6/qtdeclarative/examples/quick/tutorials/samegame/samegame3/pics/$(notdir $@) $@


qt6_samegame.tar: $(addprefix samegame/, $(SAMEGAME3_RESOURCES)) \
                  $(addprefix samegame/pics/, $(SAMEGAME_RESOURCES))
	tar --owner=0 --group=0 --numeric-owner --mode='go=' --mtime='1970-01-01 00:00+00' -cf $@ -C samegame .
	rm -rf samegame/
