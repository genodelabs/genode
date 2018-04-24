content: player_play.png \
         player_pause.png \
         player_stop.png \
         volume.png

PORT_DIR := $(call port_dir,$(REP_DIR)/ports/qt5)

player_play.png player_pause.png player_stop.png:
	cp $(PORT_DIR)/src/lib/qt5/qt5/qtbase/examples/network/torrent/icons/$@ $@

volume.png:
	cp $(PORT_DIR)/src/lib/qt5/qt5/qtwebkit/Source/WebCore/platform/efl/DefaultTheme/widget/mediacontrol/mutebutton/unmutebutton.png $@
