include $(call select_from_repositories,lib/import/import-qt5_virtualkeyboard.mk)

SHARED_LIB = yes

include $(REP_DIR)/lib/mk/qt5_virtualkeyboard_generated.inc

QT_VPATH += qtvirtualkeyboard/src/virtualkeyboard/content \
            qtvirtualkeyboard/src/virtualkeyboard/content/styles/default \
            qtvirtualkeyboard/src/virtualkeyboard/content/styles/retro

include $(REP_DIR)/lib/mk/qt5.inc

LIBS += qt5_core qt5_gui qt5_network qt5_qml qt5_quick

CC_CXX_WARN_STRICT =
