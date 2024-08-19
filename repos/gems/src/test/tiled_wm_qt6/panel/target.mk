QMAKE_PROJECT_FILE    = $(PRG_DIR)/panel.pro
QMAKE_TARGET_BINARIES = test-tiled_wm-panel

qmake_prepared.tag: icon.h

include $(PRG_DIR)/../target.inc
