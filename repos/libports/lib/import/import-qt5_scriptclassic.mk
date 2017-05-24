IMPORT_QT5_INC=$(call select_from_repositories,lib/import/import-qt5.inc)

include $(IMPORT_QT5_INC)

QT5_INC_DIR += $(QT5_PORT_DIR)/src/lib/qt5/qtscriptclassic-1.0_1-opensource/include \
               $(QT5_PORT_DIR)/src/lib/qt5/qtscriptclassic-1.0_1-opensource/include/QtScript \
               $(QT5_PORT_DIR)/src/lib/qt5/qtscriptclassic-1.0_1-opensource/src \

QT_DEFINES += -DQ_SCRIPT_EXPORT=
