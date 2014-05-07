include $(REP_DIR)/lib/import/import-qt5_qtquick2plugin.mk

SHARED_LIB = yes

include $(REP_DIR)/lib/mk/qt5_qtquick2plugin_generated.inc

QT_DEFINES += -UQT_STATICPLUGIN

include $(REP_DIR)/lib/mk/qt5.inc

LIBS += qt5_qml

# install the QtQuick QML plugin

QTQUICK_INSTALL_DIR := $(BUILD_BASE_DIR)/bin/qt5_fs/qt5/qml/QtQuick.2
QTQUICK_QMLDIR      := $(QTQUICK_INSTALL_DIR)/qmldir
QTQUICK_PLUGIN_NAME := qt5_qtquick2plugin.lib.so
QTQUICK_PLUGIN      := $(QTQUICK_INSTALL_DIR)/$(QTQUICK_PLUGIN_NAME)

$(QTQUICK_INSTALL_DIR):
	$(VERBOSE)mkdir -p $@

$(QTQUICK_QMLDIR): $(QTQUICK_INSTALL_DIR)
	$(VERBOSE)cp $(REP_DIR)/contrib/$(QT5)/qtdeclarative/src/imports/qtquick2/qmldir $(QTQUICK_INSTALL_DIR)

$(QTQUICK_PLUGIN): $(QTQUICK_INSTALL_DIR)
	$(VERBOSE)ln -sf $(BUILD_BASE_DIR)/bin/$(QTQUICK_PLUGIN_NAME) $(QTQUICK_INSTALL_DIR)/$(QTQUICK_PLUGIN_NAME)

all: $(QTQUICK_QMLDIR) $(QTQUICK_PLUGIN)

#
# unfortunately, these clean rules don't trigger
#

clean-qtquick_install_dir:
	rm -rf $(QTQUICK_INSTALL_DIR)

clean: clean-qtquick_install_dir

