IMPORT_QT5_INC=$(call select_from_repositories,lib/import/import-qt5.inc)

include $(IMPORT_QT5_INC)

QT5_INC_DIR += $(QT5_REP_DIR)/contrib/$(QT5)/qtdeclarative/include
QT5_INC_DIR += $(QT5_REP_DIR)/contrib/$(QT5)/qtdeclarative/include/QtQuick

