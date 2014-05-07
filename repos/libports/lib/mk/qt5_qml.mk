include $(REP_DIR)/lib/import/import-qt5_qml.mk

SHARED_LIB = yes

include $(REP_DIR)/lib/mk/qt5_qml_generated.inc

include $(REP_DIR)/lib/mk/qt5.inc

#INC_DIR += $(REP_DIR)/include/qt5/qtbase/QtXml/private \
#           $(REP_DIR)/contrib/$(QT5)/qtbase/include/QtCore/$(QT_VERSION)/QtCore \

LIBS += qt5_v8 qt5_core libc
