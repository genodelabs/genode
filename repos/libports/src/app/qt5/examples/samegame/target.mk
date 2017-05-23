# identify the qt5 repository by searching for a file that is unique for qt5
QT5_REP_DIR := $(call select_from_repositories,lib/import/import-qt5.inc)
QT5_REP_DIR := $(realpath $(dir $(QT5_REP_DIR))../..)

include $(QT5_REP_DIR)/src/app/qt5/tmpl/target_defaults.inc

include $(QT5_REP_DIR)/src/app/qt5/tmpl/target_final.inc

#
# install contrib resources
#

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

$(TARGET): $(addprefix $(BUILD_BASE_DIR)/bin/qt5_fs/$(TARGET)/, $(SAMEGAME3_RESOURCES)) \
           $(addprefix $(BUILD_BASE_DIR)/bin/qt5_fs/$(TARGET)/shared/pics/, $(SAMEGAME_RESOURCES))

$(BUILD_BASE_DIR)/bin/qt5_fs/$(TARGET):
	$(VERBOSE)mkdir -p $@

$(BUILD_BASE_DIR)/bin/qt5_fs/$(TARGET)/shared/pics:
	$(VERBOSE)mkdir -p $@

$(addprefix $(BUILD_BASE_DIR)/bin/qt5_fs/$(TARGET)/, $(SAMEGAME3_RESOURCES)): $(BUILD_BASE_DIR)/bin/qt5_fs/$(TARGET)
	$(VERBOSE)ln -sf $(QT5_CONTRIB_DIR)/qtdeclarative/examples/quick/tutorials/samegame/samegame3/$(notdir $@) $@

$(addprefix $(BUILD_BASE_DIR)/bin/qt5_fs/$(TARGET)/shared/pics/, $(SAMEGAME_RESOURCES)): $(BUILD_BASE_DIR)/bin/qt5_fs/$(TARGET)/shared/pics
	$(VERBOSE)ln -sf $(QT5_CONTRIB_DIR)/qtdeclarative/examples/quick/tutorials/samegame/shared/pics/$(notdir $@) $@

LIBS += qt5_component
