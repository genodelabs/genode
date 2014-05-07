include $(REP_DIR)/lib/import/import-qt5_quick.mk

SHARED_LIB = yes

include $(REP_DIR)/lib/mk/qt5_quick_generated.inc

QT_INCPATH += qtdeclarative/src/quick/items

include $(REP_DIR)/lib/mk/qt5.inc

LIBS += qt5_qml
