IMPORT_QT5_INC=$(call select_from_repositories,lib/import/import-qt5.inc)

include $(IMPORT_QT5_INC)

ifeq ($(CONTRIB_DIR),)
INC_DIR += $(call select_from_repositories,include/qt5/qpa_nitpicker) \
           $(call select_from_repositories,include/QtInputSupport/$(QT_VERSION))
else
INC_DIR += $(REP_DIR)/include/qt5/qpa_nitpicker \
           $(QT5_PORT_DIR)/include/QtInputSupport/$(QT_VERSION)
endif
