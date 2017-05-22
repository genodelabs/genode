# identify the qt repository by searching for a file that is unique for qt
QT5_REP_DIR := $(call select_from_repositories,lib/import/import-qt5.inc)
QT5_REP_DIR := $(realpath $(dir $(QT5_REP_DIR))../..)

include $(QT5_REP_DIR)/src/app/qt5/tmpl/target_defaults.inc

include $(QT5_REP_DIR)/src/app/qt5/tmpl/target_final.inc

LIBS += qt5_qnitpickerviewwidget qoost

#
# install contrib resources
#

$(TARGET): $(BUILD_BASE_DIR)/bin/qt5_fs/$(TARGET)/player_play.png \
           $(BUILD_BASE_DIR)/bin/qt5_fs/$(TARGET)/player_pause.png \
           $(BUILD_BASE_DIR)/bin/qt5_fs/$(TARGET)/player_stop.png \
           $(BUILD_BASE_DIR)/bin/qt5_fs/$(TARGET)/volume.png

$(BUILD_BASE_DIR)/bin/qt5_fs/$(TARGET):
	$(VERBOSE)mkdir -p $@

$(BUILD_BASE_DIR)/bin/qt5_fs/$(TARGET)/player_play.png \
$(BUILD_BASE_DIR)/bin/qt5_fs/$(TARGET)/player_pause.png \
$(BUILD_BASE_DIR)/bin/qt5_fs/$(TARGET)/player_stop.png: $(BUILD_BASE_DIR)/bin/qt5_fs/$(TARGET)
	$(VERBOSE)ln -sf $(QT5_CONTRIB_DIR)/qtbase/examples/network/torrent/icons/$(notdir $@) $@

$(BUILD_BASE_DIR)/bin/qt5_fs/$(TARGET)/volume.png: $(BUILD_BASE_DIR)/bin/qt5_fs/$(TARGET)
	$(VERBOSE)ln -sf $(QT5_CONTRIB_DIR)/qtwebkit/Source/WebCore/platform/efl/DefaultTheme/widget/mediacontrol/mutebutton/unmutebutton.png $@
