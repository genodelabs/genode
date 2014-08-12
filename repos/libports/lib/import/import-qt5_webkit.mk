IMPORT_QT5_INC=$(call select_from_repositories,lib/import/import-qt5.inc)

include $(IMPORT_QT5_INC)

QT5_INC_DIR += $(QT5_REP_DIR)/include/qt5/qtwebkit \
               $(QT5_CONTRIB_DIR)/qtwebkit/include \
               $(QT5_REP_DIR)/include/qt5/qtwebkit/QtWebKit \
               $(QT5_CONTRIB_DIR)/qtwebkit/include/QtWebKit \
