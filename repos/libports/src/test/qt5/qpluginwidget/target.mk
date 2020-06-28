QMAKE_PROJECT_FILE = $(PRG_DIR)/qpluginwidget.pro

QMAKE_TARGET_BINARIES = test-qpluginwidget

QT5_PORT_LIBS = libQt5Core libQt5Gui libQt5Network libQt5Widgets

LIBS = libc libm mesa qt5_component stdcxx libqgenodeviewwidget libqpluginwidget qoost $(QT5_PORT_LIBS)

include $(call select_from_repositories,lib/import/import-qt5_qmake.mk)

#
# create tar archive for test plugin
#

TEST_PLUGIN_TAR = $(BUILD_BASE_DIR)/bin/test-plugin.tar

$(TARGET): $(TEST_PLUGIN_TAR)

$(TEST_PLUGIN_TAR): config.plugin
	$(VERBOSE)tar cf $@ -C $(PRG_DIR) config.plugin

clean:
	$(VERBOSE)rm $(TEST_PLUGIN_TAR)
